//  These codes are used to implement the Hybrid Scheduler class, the backbones
//  of the code are from the ghOSt project. The Hybrid Scheduler is a
//  combination of the FIFO and the CFS policies. The FIFO and CFS policies are
//  based on the implementation of the ghOSt project. More details can be found
//  in schedulers/fifo and schedulers/cfs.

#ifndef GHOST_SCHEDULERS_HYBRID_HYBRID_SCHEDULER_H
#define GHOST_SCHEDULERS_HYBRID_HYBRID_SCHEDULER_H

#include <cstdint>
#include <map>
#include <memory>

#include "absl/time/time.h"
#include "lib/agent.h"
#include "lib/scheduler.h"
#include "schedulers/hybrid/cfs_scheduler.h"

namespace ghost {

class CircularBuffer {
   private:
    std::vector<bool> buffer;
    size_t head = 0;
    size_t count = 0;
    size_t capacity;
    int trueCount = 0;

   public:
    CircularBuffer(size_t size) : capacity(size), buffer(size, false) {}

    void push(bool value) {
        // if head is true, we need replace head with value, so decreses
        // trueCount
        if (buffer[head]) {
            trueCount--;
        }
        buffer[head] = value;
        if (value) {
            trueCount++;
        }
        head = (head + 1) % capacity;  // move head to next position

        if (count < capacity) {
            count++;
        }
    }

    int countTrue() const { return trueCount; }
    int size() const { return count; }
    bool bufferIsFull() const { return count == capacity && head == 0; }
};

enum class ShortQueueTaskState {
    kBlocked,   // not on runqueue.
    kRunnable,  // transitory state:
                // 1. kBlocked->kRunnable->kQueued
                // 2. kQueued->kRunnable->kOnCpu
    kQueued,    // on runqueue.
    kOnCpu,     // running on cpu.
    kYielding,  // yielding on cpu.

    // kBlocked: task is not ready
    // kRunnable: task is ready, but not in runqueue
    // kQueued: task is ready and in runqueue
    // kOnCpu: task is running on cpu
};

std::ostream& operator<<(std::ostream& os, const ShortQueueTaskState& state);

struct ShortQueueTask : public Task<> {
    explicit ShortQueueTask(Gtid hybrid_task_gtid, ghost_sw_info sw_info)
        : Task<>(hybrid_task_gtid, sw_info) {}
    ~ShortQueueTask() override {}

    inline bool blocked() const {
        return run_state == ShortQueueTaskState::kBlocked;
    }
    inline bool queued() const {
        return run_state == ShortQueueTaskState::kQueued;
    }
    inline bool oncpu() const {
        return run_state == ShortQueueTaskState::kOnCpu;
    }
    inline bool runnable() const {
        return run_state == ShortQueueTaskState::kRunnable;
    }
    inline bool yielding() const {
        return run_state == ShortQueueTaskState::kYielding;
    }

    void UpdateRuntime();

    ShortQueueTaskState run_state = ShortQueueTaskState::kBlocked;
    absl::Duration elapsed_runtime = absl::ZeroDuration();
    Cpu cpu{Cpu::UninitializedType::kUninitialized};

    bool preempted = false;
    bool prio_boost = false;
    bool has_run = false;
    std::atomic<bool> new_to_cfs{true};
    bool sampled = false;  // for sampling the portion of the task that is
                           // completed in preemption time slice
};

class ShortQueueRq {
   public:
    ShortQueueRq() = default;
    ShortQueueRq(const ShortQueueRq&) = delete;
    ShortQueueRq& operator=(ShortQueueRq&) = delete;

    ShortQueueTask* Dequeue();
    void Enqueue(ShortQueueTask* task);

    size_t RunqueueSize() const {
        absl::MutexLock lock(&mu_);
        return run_queue_.size();
    }

    bool RunqueueEmpty() const { return RunqueueSize() == 0; }

    void RemoveFromRunqueue(ShortQueueTask* task);

    void Show();

   private:
    mutable absl::Mutex mu_;
    std::deque<ShortQueueTask*> run_queue_ ABSL_GUARDED_BY(mu_);
};

class HybridScheduler : public BasicDispatchScheduler<ShortQueueTask> {
   public:
    explicit HybridScheduler(
        Enclave* enclave, CpuList cpulist,
        std::shared_ptr<TaskAllocator<ShortQueueTask>> allocator,
        int32_t global_cpu, CpuList short_cpulist,
        absl::Duration preemption_time_slice);
    ~HybridScheduler() final;

    void ShortQueueSchedule(const StatusWord& agent_sw,
                            BarrierToken agent_sw_last);

    void EnclaveReady() final;
    Channel& GetDefaultChannel() final { return global_channel_; };

    bool Empty() { return num_tasks_ == 0 && Empty(long_cpulist_); }

    // overhead is too high
    bool Empty(CpuList cpulist) {
        for (const Cpu& cpu : cpulist) {
            CfsCpuState* cs = cfs_cpu_state(cpu);
            absl::MutexLock l(&cs->run_queue.mu_);
            if (!cs->run_queue.IsEmpty()) {
                return false;
            }
        }
        return true;
    }

    void DumpState(const Cpu& cpu, int flags) final;
    std::atomic<bool> debug_runqueue_{false};

    int CountAllTasks() {
        std::cout << "Short queue size: " << short_queue_.RunqueueSize()
                  << std::endl;
        return short_queue_.RunqueueSize();
    }

    int32_t GetGlobalCPUId() {
        return global_cpu_.load(std::memory_order_acquire);
    }

    void SetGlobalCPU(const Cpu& cpu) {
        global_cpu_.store(cpu.id(), std::memory_order_release);
    }

    bool PickNextGlobalCPU(BarrierToken agent_barrier, const Cpu& this_cpu);

    CpuList GetShortCPUList() {
        absl::MutexLock lock(&cpulist_mu_);
        return short_cpulist_;
    }

    CpuList GetLongCPUList() {
        absl::MutexLock lock(&cpulist_mu_);
        return long_cpulist_;
    }

    absl::Duration GetPreemptionTimeSlice() {
        absl::MutexLock lock(&mu_);
        return preemption_time_slice_;
    }

    void LogPreemptionTimeSlice();

    void LogCPUList();

    void AddLongCPUList();

    void AddShortCPUList();

    float GetShortCPULoad();
    float GetLongCPULoad();
    uint32_t GetCPULoad(uint32_t cpu_id);
    std::vector<float> ReadSharedMemory(const char* shm_name,
                                        const char* sem_name, int start_idx,
                                        int end_idx);

    void GetSlidingWindowPreemptionTimeSlice(uint32_t core_number);
    void AdaptPreemptionByDuration();
    uint32_t sliding_window_size_ = 100;
    uint32_t duration_window_size_ = 100;
    CircularBuffer ifFinishInTime{100};
    int sampling_num = 10;

    uint32_t GetShortQueueSize() { return short_queue_.RunqueueSize(); }
    uint32_t GetShortCPUSize();
    uint32_t GetCPUSize();
    mutable int average_IAT_;
    std::deque<int> sliding_window_;

    void SetShortCPUList(const CpuList& cpulist) {
        for (const Cpu& cpu : cpulist) {
            short_cpulist_.Set(cpu);
        }
    }

    CfsTask* ConvertToCfsTask(ShortQueueTask* task, const Message& msg);
    void TaskNewToCfs(ShortQueueTask* task, CfsTask* CfsTask,
                      const Message& msg);
    void HandleTaskDone(CfsTask* task, bool from_switchto);

    static constexpr int kDebugRunqueue = 1;
    static constexpr int kCountAllTasks = 2;
    static std::unordered_map<int, int> preempt_count_per_cpu;
    static std::vector<int> preemption_time_slice_list;
    static std::vector<int> short_cpu_num_list;

   protected:
    void TaskNew(ShortQueueTask* task, const Message& msg) final;
    void TaskRunnable(ShortQueueTask* task, const Message& msg) final;
    void TaskDeparted(ShortQueueTask* task, const Message& msg) final;
    void TaskDead(ShortQueueTask* task, const Message& msg) final;
    void TaskYield(ShortQueueTask* task, const Message& msg) final;
    void TaskBlocked(ShortQueueTask* task, const Message& msg) final;
    void TaskPreempted(ShortQueueTask* task, const Message& msg) final;

   private:
    struct CpuState {
        ShortQueueTask* current = nullptr;
        const Agent* agent = nullptr;
        absl::Time last_commit;
        std::atomic<bool> blocked{false};
    } ABSL_CACHELINE_ALIGNED;

    void TaskOffCpu(ShortQueueTask* task, bool blocked);
    void TaskOnCpu(ShortQueueTask* task, const Cpu& cpu);
    void Yield(ShortQueueTask* task);
    void Unyield(ShortQueueTask* task);
    void DumpAllTasks();
    bool Available(const Cpu& cpu);

    inline CpuState* cpu_state(const Cpu& cpu) {
        return &cpu_states_[cpu.id()];
    }

    CpuState* cpu_state_of(const ShortQueueTask* task);

    CpuState cpu_states_[MAX_CPUS];

    mutable CpuList short_cpulist_;
    mutable CpuList long_cpulist_ = cpus() - short_cpulist_;
    mutable absl::Mutex cpulist_mu_;

    LocalChannel global_channel_;
    std::atomic<int32_t> global_cpu_;
    std::atomic<int> num_tasks_{0};

    mutable absl::Duration preemption_time_slice_;
    mutable absl::Mutex mu_;

    ShortQueueRq short_queue_;
    std::vector<ShortQueueTask*> yielding_tasks_;
    uint64_t iterations_ = 0;

    // cfs
    static constexpr int kMaxNice = 19;
    static constexpr int kMinNice = -20;
    static constexpr uint32_t kNiceToWeight[40] = {
        88761, 71755, 56483, 46273, 36291,  // -20 .. -16
        29154, 23254, 18705, 14949, 11916,  // -15 .. -11
        9548,  7620,  6100,  4904,  3906,   // -10 .. -6
        3121,  2501,  1991,  1586,  1277,   // -5 .. -1
        1024,  820,   655,   526,   423,    // 0 .. 4
        335,   272,   215,   172,   137,    // 5 .. 9
        110,   87,    70,    56,    45,     // 10 .. 14
        36,    29,    23,    18,    15      // 15 .. 19
    };
    static constexpr uint32_t kNiceToInverseWeight[40] = {
        48388,     59856,     76040,     92818,     118348,    // -20 .. -16
        147320,    184698,    229616,    287308,    360437,    // -15 .. -11
        449829,    563644,    704093,    875809,    1099582,   // -10 .. -6
        1376151,   1717300,   2157191,   2708050,   3363326,   // -5 .. -1
        4194304,   5237765,   6557202,   8165337,   10153587,  // 0 .. 4
        12820798,  15790321,  19976592,  24970740,  31350126,  // 5 .. 9
        39045157,  49367440,  61356676,  76695844,  95443717,  // 10 .. 14
        119304647, 148102320, 186737708, 238609294, 286331153  // 15 .. 19
    };
    CfsCpuState* cfs_cpu_state(const Cpu& cpu) {
        return &cfs_cpu_states_[cpu.id()];
    }

    CfsCpuState* cfs_cpu_state_of(const CfsTask* task) {
        CHECK_GE(task->cpu, 0);
        CHECK_LT(task->cpu, MAX_CPUS);
        return &cfs_cpu_states_[task->cpu];
    }

    int MyCpu(bool is_agent_thread = true) {
        if (!is_agent_thread) return sched_getcpu();
        static thread_local int my_cpu = -1;
        if (my_cpu == -1) {
            my_cpu = sched_getcpu();
        }
        return my_cpu;
    }

    CfsCpuState cfs_cpu_states_[MAX_CPUS];
    std::unique_ptr<TaskAllocator<CfsTask>> cfs_allocator_;
    bool idle_load_balancing_;

    inline CfsTask* NewIdleBalance(CfsCpuState* cs);

    enum class CpuIdleType : uint32_t {
        kCpuIdle = 0,   // The CPU is idle
        kCpuNotIdle,    // The CPU is not idle
        kCpuNewlyIdle,  // The CPU is going to be idle
        kNumCpuIdleType,
    };

    static constexpr size_t kMaxTasksToLoadBalance = 32;

    struct LoadBalanceEnv {
        CfsCpuState* dst_cs = nullptr;
        CfsCpuState* src_cs = nullptr;
        int imbalance = 0;
        std::vector<CfsTask*> tasks;
        CpuIdleType idle;
    };

    int LoadBalance(CfsCpuState* cs, CpuIdleType idle_type);
    inline void AttachTasks(struct LoadBalanceEnv& env);
    inline int DetachTasks(struct LoadBalanceEnv& env);
    inline int CalculateImbalance(LoadBalanceEnv& env);
    inline int FindBusiestQueue();
    inline bool ShouldWeBalance(LoadBalanceEnv& env);
    void PutPrevTask();
    void StartMigrateTask(CfsTask* task);
    void StartMigrateTask(CfsCpuState* cs);
    Cpu SelectTaskRq(CfsTask* task);
    void PingCpu(const Cpu& cpu);
    bool Migrate(CfsTask* task, Cpu cpu, BarrierToken seqnum);
    void MigrateTasks(CfsCpuState* cs);

    absl::Duration min_granularity_ = absl::Milliseconds(1);
    absl::Duration latency_ = absl::Milliseconds(10);
};

std::unique_ptr<HybridScheduler> SingleThreadedHybridScheduler(
    Enclave* enclave, CpuList cpulist, int32_t global_cpu,
    CpuList short_cpulist, absl::Duration preemption_time_slice);

class HybridAgent : public LocalAgent {
   public:
    HybridAgent(Enclave* enclave, Cpu cpu, HybridScheduler* scheduler)
        : LocalAgent(enclave, cpu), scheduler_(scheduler) {}

    void AgentThread() override;
    Scheduler* AgentScheduler() const override { return scheduler_; }

   private:
    HybridScheduler* scheduler_;
};

class ShortQueueConfig : public AgentConfig {
   public:
    ShortQueueConfig() {}
    ShortQueueConfig(Topology* topology, CpuList cpulist, Cpu global_cpu,
                     CpuList short_cpulist,
                     absl::Duration preemption_time_slice)
        : AgentConfig(topology, std::move(cpulist)),
          global_cpu_(global_cpu),
          short_cpulist_(std::move(short_cpulist)),
          preemption_time_slice_(preemption_time_slice) {}

    Cpu global_cpu_{Cpu::UninitializedType::kUninitialized};
    CpuList short_cpulist_ = MachineTopology()->EmptyCpuList();
    absl::Duration preemption_time_slice_ = absl::InfiniteDuration();
};

template <class EnclaveType>
class FullHybridAgent : public FullAgent<EnclaveType> {
   public:
    explicit FullHybridAgent(ShortQueueConfig config)
        : FullAgent<EnclaveType>(config) {
        scheduler_ = SingleThreadedHybridScheduler(
            &this->enclave_, *this->enclave_.cpus(), config.global_cpu_.id(),
            config.short_cpulist_, config.preemption_time_slice_);
        this->StartAgentTasks();
        this->enclave_.Ready();
    }

    ~FullHybridAgent() override {
        auto global_cpuid = scheduler_->GetGlobalCPUId();

        if (this->agents_.front()->cpu().id() != global_cpuid) {
            for (auto it = this->agents_.begin(); it != this->agents_.end();
                 it++) {
                if (((*it)->cpu().id() == global_cpuid)) {
                    auto d = std::distance(this->agents_.begin(), it);
                    std::iter_swap(this->agents_.begin(),
                                   this->agents_.begin() + d);
                    break;
                }
            }
        }

        CHECK_EQ(this->agents_.front()->cpu().id(), global_cpuid);

        this->TerminateAgentTasks();
    }

    std::unique_ptr<Agent> MakeAgent(const Cpu& cpu) override {
        return std::make_unique<HybridAgent>(&this->enclave_, cpu,
                                             scheduler_.get());
    }

    // todo pass enclave to the constructor
    // Enclave* GetEnclave() { return &this->enclave_; }

    void RpcHandler(int64_t req, const AgentRpcArgs& args,
                    AgentRpcResponse& response) override {
        switch (req) {
            case HybridScheduler::kDebugRunqueue:
                scheduler_->debug_runqueue_ = true;
                response.response_code = 0;
                return;
            case HybridScheduler::kCountAllTasks:
                response.response_code = scheduler_->CountAllTasks();
                return;
            default:
                response.response_code = -1;
                return;
        }
    }

   private:
    std::unique_ptr<HybridScheduler> scheduler_;
};

}  // namespace ghost

#endif

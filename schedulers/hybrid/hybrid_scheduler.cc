//  These codes are used to implement the Hybrid Scheduler class, the backbones
//  of the code are from the Ghost project. The Hybrid Scheduler is a
//  combination of the FIFO and the CFS policies.

#include "schedulers/hybrid/hybrid_scheduler.h"

#include <semaphore.h>

#include <memory>

#include "schedulers/hybrid/cfs_scheduler.h"

#define DPRINT_CFS(level, message)                                   \
    do {                                                             \
        if (ABSL_PREDICT_TRUE(verbose() < level)) break;             \
        absl::FPrintF(stderr, "DCFS: [%.6f] cpu %d: %s\n",           \
                      absl::ToDoubleSeconds(MonotonicNow() - start), \
                      sched_getcpu(), message);                      \
    } while (0)

namespace ghost {

void PrintDebugTaskMessage(std::string message_name, CfsCpuState* cs,
                           CfsTask* task);

// update the runtime of the task
void ShortQueueTask::UpdateRuntime() {
    int ret = GhostHelper()->GetTaskRuntime(this->gtid, this->elapsed_runtime);
}

// constructor
HybridScheduler::HybridScheduler(
    Enclave* enclave, CpuList cpulist,
    std::shared_ptr<TaskAllocator<ShortQueueTask>> allocator,
    int32_t global_cpu, CpuList short_cpulist,
    absl::Duration preemption_time_slice)
    : BasicDispatchScheduler(enclave, std::move(cpulist), std::move(allocator)),
      global_cpu_(global_cpu),
      short_cpulist_(short_cpulist),
      global_channel_(GHOST_MAX_QUEUE_ELEMS, 0),
      preemption_time_slice_(preemption_time_slice) {
    if (!cpus().IsSet(global_cpu_)) {
        Cpu c = cpus().Front();
        CHECK(c.valid());
        global_cpu_ = c.id();
    }
    cfs_allocator_ = std::make_unique<ThreadSafeMallocTaskAllocator<CfsTask>>();
}

HybridScheduler::~HybridScheduler() {}

void HybridScheduler::EnclaveReady() {
    for (const Cpu& cpu : cpus()) {
        CpuState* cs = cpu_state(cpu);
        cs->agent = enclave()->GetAgent(cpu);
        CHECK_NE(cs->agent, nullptr);
    }
}

bool HybridScheduler::Available(const Cpu& cpu) {
    CpuState* cs = cpu_state(cpu);

    if (cs->agent) return cs->agent->cpu_avail();

    return false;
}

void HybridScheduler::DumpAllTasks() {
    fprintf(stderr, "task        state   cpu\n");
    allocator()->ForEachTask([](Gtid gtid, const ShortQueueTask* task) {
        absl::FPrintF(stderr, "%-12s%-8d%-d\n", gtid.describe(),
                      task->run_state, task->cpu.valid() ? task->cpu.id() : -1);
        return true;
    });
}

void HybridScheduler::DumpState(const Cpu& cpu, int flags) {
    if (flags & Scheduler::kDumpAllTasks) {
        DumpAllTasks();
    }

    if (!(flags & Scheduler::kDumpStateEmptyRQ) &&
        short_queue_.RunqueueEmpty()) {
        return;
    }

    fprintf(stderr, "SchedState: ");
    for (const Cpu& cpu : cpus()) {
        CpuState* cs = cpu_state(cpu);
        fprintf(stderr, "%d:", cpu.id());
        if (!cs->current) {
            fprintf(stderr, "none ");
        } else {
            Gtid gtid = cs->current->gtid;
            absl::FPrintF(stderr, "%s ", gtid.describe());
        }
    }
    fprintf(stderr, " rq_l=%ld", short_queue_.RunqueueSize());
    fprintf(stderr, "\n");
}

// get the cpu state of the task
HybridScheduler::CpuState* HybridScheduler::cpu_state_of(
    const ShortQueueTask* task) {
    CHECK(task->cpu.valid());
    CHECK(task->oncpu());
    CpuState* cs = cpu_state(task->cpu);
    CHECK(task == cs->current);
    return cs;
}

// task is created in ghost
absl::Time last_task_new = absl::Now();
void HybridScheduler::TaskNew(ShortQueueTask* task, const Message& msg) {
    absl::Time now = absl::Now();
    std::ofstream outfile;
    outfile.open("project/log/metrics_Hybrid_firecracker.txt", std::ios::app);
    outfile << "TaskNew: " << task->gtid.describe()
            << " enqueue in short queue at " << now << std::endl;
    outfile.close();
    const ghost_msg_payload_task_new* payload =
        static_cast<const ghost_msg_payload_task_new*>(msg.payload());

    DCHECK_EQ(payload->runtime, task->status_word.runtime());

    task->seqnum = msg.seqnum();
    task->run_state = ShortQueueTaskState::kBlocked;

    const Gtid gtid(payload->gtid);
    if (payload->runnable) {
        task->run_state = ShortQueueTaskState::kRunnable;
        short_queue_.Enqueue(task);
    } else {
        // Wait until task becomes runnable to avoid race between migration
        // and MSG_TASK_WAKEUP showing up on the default channel.
    }

    num_tasks_.fetch_add(1, std::memory_order_relaxed);

    // sliding window
    // int IAT = absl::ToInt64Milliseconds(now - last_task_new);
    // if (sliding_window_.size() < sliding_window_size_) {
    //     sliding_window_.push_back(IAT);
    // } else {
    //     sliding_window_.pop_front();
    //     sliding_window_.push_back(IAT);
    // }
    // last_task_new = now;
}

// task is runnable
void HybridScheduler::TaskRunnable(ShortQueueTask* task, const Message& msg) {
    const ghost_msg_payload_task_wakeup* payload =
        static_cast<const ghost_msg_payload_task_wakeup*>(msg.payload());

    CHECK(task->blocked());
    task->run_state = ShortQueueTaskState::kRunnable;

    task->prio_boost = !payload->deferrable;

    task->prio_boost = false;
    short_queue_.Enqueue(task);
}

// task departed from ghost
void HybridScheduler::TaskDeparted(ShortQueueTask* task, const Message& msg) {
    if (task->yielding()) {
        Unyield(task);
    }
    if (task->oncpu()) {
        CpuState* cs = cpu_state_of(task);
        CHECK_EQ(cs->current, task);
        cs->current = nullptr;
    } else if (task->queued()) {
        printf("Task %s is queued \n", task->gtid.describe());
        short_queue_.RemoveFromRunqueue(task);
    } else {
        CHECK(task->blocked());
    }

    allocator()->FreeTask(task);
    num_tasks_.fetch_sub(1, std::memory_order_relaxed);
}

// calculate the percentile of the duration
int calculatePercentile(std::deque<int>& data) {
    if (data.empty()) return 0;

    std::vector<int> temp(data.begin(), data.end());
    size_t n = temp.size();
    size_t index = static_cast<size_t>(0.95 * n);
    std::nth_element(temp.begin(), temp.begin() + index, temp.end());
    return temp[index];
}

// task is dead, get the duration of the task to adapt the preemption time slice
std::atomic<int> task_count(0);
std::deque<int> duration_window_;
bool adapt_preemption_by_duration = true;
void HybridScheduler::TaskDead(ShortQueueTask* task, const Message& msg) {
    task->UpdateRuntime();
    CHECK(task->blocked());
    std::ofstream outfile;
    absl::Time now = absl::Now();
    outfile.open("project/log/metrics_Hybrid_firecracker.txt", std::ios::app);
    outfile << "TaskDead: " << task->gtid.describe() << " at " << now
            << " runs for " << task->elapsed_runtime << " in total"
            << std::endl;
    outfile.close();
    if (adapt_preemption_by_duration) {
        int duration_ms = absl::ToInt64Milliseconds(task->elapsed_runtime);

        if (duration_window_.size() >= duration_window_size_) {
            duration_window_.pop_front();
        }
        duration_window_.push_back(duration_ms);
    }

    if (task->elapsed_runtime < GetPreemptionTimeSlice()) {
        CHECK(task->blocked());
        allocator()->FreeTask(task);
    } else {
        CfsTask* cfsTask = new CfsTask(task->gtid, task->status_word.sw_info());
        cfsTask->elapsed_runtime = task->elapsed_runtime;
        cfsTask->Advance(task->seqnum + 1);
        cfsTask->cpu = task->cpu.id();
        CfsCpuState* cs = cfs_cpu_state_of(cfsTask);
        PrintDebugTaskMessage("TaskDead", cs, cfsTask);
        absl::MutexLock lock(&cs->run_queue.mu_);
        cs->run_queue.mu_.AssertHeld();
        HandleTaskDone(cfsTask, false);
    }

    num_tasks_.fetch_sub(1, std::memory_order_relaxed);
    task_count.fetch_add(1, std::memory_order_relaxed);
    std::cout << "task_dead: " << task_count << std::endl;
    std::cout << "number of tasks in ghost: " << num_tasks_ << std::endl;
}

// task is blocked
void HybridScheduler::TaskBlocked(ShortQueueTask* task, const Message& msg) {
    task->UpdateRuntime();
    if (task->elapsed_runtime < GetPreemptionTimeSlice() &&
        task->new_to_cfs.load(std::memory_order_relaxed)) {
        if (task->oncpu()) {
            CpuState* cs = cpu_state_of(task);
            CHECK_EQ(cs->current, task);
            cs->current = nullptr;
        } else {
            CHECK(task->queued());
            short_queue_.RemoveFromRunqueue(task);
        }
        task->run_state = ShortQueueTaskState::kBlocked;
    } else {
        CfsTask* cfsTask = ConvertToCfsTask(task, msg);
        CpuState* cs = cpu_state(task->cpu);
        cfsTask->task_state.SetState(CfsTaskState::State::kBlocked);
        task->run_state = ShortQueueTaskState::kBlocked;
    }
}

// task is preempted
std::unordered_map<int, int> HybridScheduler::preempt_count_per_cpu;
void HybridScheduler::TaskPreempted(ShortQueueTask* task, const Message& msg) {
    task->UpdateRuntime();
    if (task->elapsed_runtime < GetPreemptionTimeSlice() &&
        task->new_to_cfs.load(std::memory_order_relaxed)) {
        if (task->oncpu()) {
            CpuState* cs = cpu_state_of(task);
            CHECK_EQ(cs->current, task);
            cs->current = nullptr;
            task->run_state = ShortQueueTaskState::kRunnable;
            task->prio_boost = true;
            short_queue_.Enqueue(task);
        } else {
            CHECK(task->queued());
        }
    } else {
        task->preempted = true;
        CfsTask* cfsTask = ConvertToCfsTask(task, msg);
        TaskNewToCfs(task, cfsTask, msg);
    }
    preempt_count_per_cpu[task->cpu.id()]++;
}

void HybridScheduler::TaskYield(ShortQueueTask* task, const Message& msg) {
    if (task->oncpu()) {
        CpuState* cs = cpu_state_of(task);
        CHECK_EQ(cs->current, task);
        cs->current = nullptr;
        Yield(task);
    } else {
        CHECK(task->queued());
    }
}

void HybridScheduler::Yield(ShortQueueTask* task) {
    CHECK(task->oncpu() || task->runnable());
    task->run_state = ShortQueueTaskState::kYielding;
    yielding_tasks_.emplace_back(task);
}

void HybridScheduler::Unyield(ShortQueueTask* task) {
    CHECK(task->yielding());

    auto it = std::find(yielding_tasks_.begin(), yielding_tasks_.end(), task);
    CHECK(it != yielding_tasks_.end());
    yielding_tasks_.erase(it);

    task->run_state = ShortQueueTaskState::kRunnable;
}

void HybridScheduler::TaskOffCpu(ShortQueueTask* task, bool blocked) {
    GHOST_DPRINT(3, stderr, "Task %s offcpu %d", task->gtid.describe(),
                 task->cpu.id());
    CpuState* cs = cpu_state_of(task);

    if (task->oncpu()) {
        CHECK_EQ(cs->current, task);
        cs->current = nullptr;
    } else {
        CHECK_EQ(task->run_state, ShortQueueTaskState::kBlocked);
    }

    task->run_state = blocked ? ShortQueueTaskState::kBlocked
                              : ShortQueueTaskState::kRunnable;
}

// task on short cpu
void HybridScheduler::TaskOnCpu(ShortQueueTask* task, const Cpu& cpu) {
    CpuState* cs = cpu_state(cpu);
    CHECK_EQ(task, cs->current);

    GHOST_DPRINT(3, stderr, "ShortQueueTask %s oncpu %d", task->gtid.describe(),
                 cpu.id());

    task->run_state = ShortQueueTaskState::kOnCpu;
    task->cpu = cpu;
    task->prio_boost = false;
    task->has_run = true;
}

// add a cpu core to the long cpu list
void HybridScheduler::AddLongCPUList() {
    uint32_t front_cpu = long_cpulist_.Front().id();
    uint32_t back_cpu = long_cpulist_.Back().id();
    uint32_t min_cpu = cpus().Front().id();
    uint32_t new_lower_bound = front_cpu - 1;
    uint32_t global_cpu_id = GetGlobalCPUId();

    if (new_lower_bound > min_cpu + 1 && new_lower_bound != global_cpu_id) {
        absl::MutexLock lock(&cpulist_mu_);
        long_cpulist_.Set(new_lower_bound);
        std::cout << "add long cpu" << new_lower_bound << std::endl;
        short_cpulist_ = cpus() - long_cpulist_;
    }
}

// add a cpu core to the short cpu list
void HybridScheduler::AddShortCPUList() {
    uint32_t back_cpu = short_cpulist_.Back().id();
    uint32_t max_cpu = cpus().Back().id();
    uint32_t next_cpu = back_cpu + 1;

    if (next_cpu < max_cpu - 1) {
        std::cout << "Add short CPU:" << next_cpu << std::endl;
        cpulist_mu_.Lock();
        long_cpulist_.Clear(next_cpu);
        cpulist_mu_.Unlock();
        CfsCpuState* cs = cfs_cpu_state(topology()->cpu(next_cpu));
        cs->ConvertToShort.store(true, std::memory_order_relaxed);
        if (cs->current) {
            cs->current = nullptr;
        }
        StartMigrateTask(cs);
        MigrateTasks(cs);
        cs->ConvertToShort.store(false, std::memory_order_relaxed);
        cpulist_mu_.Lock();
        short_cpulist_.Set(next_cpu);
        cpulist_mu_.Unlock();
    }
}

// get the short cpu load
float HybridScheduler::GetShortCPULoad() {
    CpuList short_cpulist = GetShortCPUList();
    if (short_cpulist.Size() == 0) {
        return 0;
    }

    uint32_t cpu_count = short_cpulist.Size();
    std::vector<float> short_cpu_util =
        ReadSharedMemory("cpu_usage", "cpu_usage_sem", 0, cpu_count - 1);

    uint32_t total_load = 0;
    for (uint32_t i = 0; i < cpu_count; ++i) {
        total_load += short_cpu_util[i];
    }

    uint32_t average_load = total_load / cpu_count;

    return average_load;
}

// get the long cpu load
float HybridScheduler::GetLongCPULoad() {
    CpuList long_cpulist = GetLongCPUList();
    if (long_cpulist.Size() == 0) {
        return 0;
    }

    uint32_t cpu_count = long_cpulist.Size();
    std::vector<float> long_cpu_util = ReadSharedMemory(
        "cpu_usage", "cpu_usage_sem", cpus().Size() - long_cpulist.Size(),
        cpus().Size() - 1);

    uint32_t total_load = 0;
    for (uint32_t i = 0; i < cpu_count; ++i) {
        total_load += long_cpu_util[i];
    }

    uint32_t average_load = total_load / cpu_count;

    return average_load;
}

// get the cpu load by a shared memory, also see
// project/function_trace/monitor.py, the overhead of this method is too high
std::vector<float> HybridScheduler::ReadSharedMemory(const char* shm_name,
                                                     const char* sem_name,
                                                     int start_idx,
                                                     int end_idx) {
    std::vector<float> cpu_usages;

    // make sure the index range is valid
    if (start_idx < 0 || end_idx < start_idx) {
        std::cerr << "Invalid index range" << std::endl;
        return cpu_usages;  // return empty vector
    }

    // open the semaphore
    sem_t* sem = sem_open(sem_name, 0);
    if (sem == SEM_FAILED) {
        perror("Failed to open semaphore");
        return cpu_usages;
    }

    // try to acquire the semaphore
    sem_wait(sem);

    int fd = shm_open(shm_name, O_RDONLY, 0666);
    if (fd == -1) {
        std::cerr << "Error opening shared memory" << std::endl;
        sem_post(sem);  // release the semaphore
        return cpu_usages;
    }

    void* mm = mmap(nullptr, getpagesize(), PROT_READ, MAP_SHARED, fd, 0);
    if (mm == MAP_FAILED) {
        perror("Error mapping memory");
        close(fd);
        sem_post(sem);  // release the semaphore
        return cpu_usages;
    }

    // count the number of elements to read
    int num_elements = end_idx - start_idx + 1;
    cpu_usages.resize(num_elements);  // allocate vector

    // read the data from shared memory
    memcpy(cpu_usages.data(),
           static_cast<char*>(mm) + start_idx * sizeof(float),
           num_elements * sizeof(float));

    // clean up
    munmap(mm, getpagesize());
    close(fd);
    sem_post(sem);  // release the semaphore

    return cpu_usages;
}

// get the size of the short cpu list
uint32_t HybridScheduler::GetShortCPUSize() {
    CpuList short_cpulist = GetShortCPUList();
    return short_cpulist.Size();
}

uint32_t HybridScheduler::GetCPUSize() { return cpus().Size(); }

// log the preemption time slice
static absl::Mutex preempt_mutex;
std::vector<int> HybridScheduler::preemption_time_slice_list;
static std::atomic<std::chrono::steady_clock::time_point> last_record_time(
    std::chrono::steady_clock::now());
void HybridScheduler::LogPreemptionTimeSlice() {
    auto now = std::chrono::steady_clock::now();
    auto last_time = last_record_time.load(std::memory_order_relaxed);

    if (now - last_time >= std::chrono::seconds(1)) {
        if (last_record_time.compare_exchange_strong(last_time, now)) {
            absl::MutexLock lock(&preempt_mutex);
            int time_slice = ToInt64Milliseconds(GetPreemptionTimeSlice());
            preemption_time_slice_list.push_back(time_slice);
        }
    }
}

// log the cpu list
static absl::Mutex list_mutex;
std::vector<int> HybridScheduler::short_cpu_num_list;
static std::atomic<std::chrono::steady_clock::time_point> last_record_time_2(
    std::chrono::steady_clock::now());
void HybridScheduler::LogCPUList() {
    auto now = std::chrono::steady_clock::now();
    auto last_time = last_record_time_2.load(std::memory_order_relaxed);

    if (now - last_time >= std::chrono::seconds(1)) {
        if (last_record_time_2.compare_exchange_strong(last_time, now)) {
            absl::MutexLock lock(&list_mutex);
            int short_cpu_num = GetShortCPUList().Size();
            short_cpu_num_list.push_back(short_cpu_num);
        }
    }
}

// get the preemption time slice
void HybridScheduler::GetSlidingWindowPreemptionTimeSlice(
    uint32_t core_number) {
    int sum = 0;
    for (auto i = sliding_window_.begin(); i != sliding_window_.end(); i++) {
        sum += *i;
    }
    // if the sliding window is not full, the preemption time slice does not
    // change, otherwise the initial preemption time slice is too large
    if (sliding_window_.size() == sliding_window_size_) {
        absl::MutexLock lock(&mu_);
        preemption_time_slice_ = absl::Milliseconds(static_cast<int>(
            std::round(sum / sliding_window_.size()) * core_number * 2));
        std::cout << "preemption_time_slice_: " << preemption_time_slice_
                  << std::endl;
    }
}

// adapt the preemption time slice by the duration of the task
std::atomic<int> count_AdaptPreemptionByDuration(0);
void HybridScheduler::AdaptPreemptionByDuration() {
    if (duration_window_.size() < duration_window_size_) {
        return;
    }

    count_AdaptPreemptionByDuration.fetch_add(1);
    if (count_AdaptPreemptionByDuration >
        duration_window_size_ * GetCPUSize()) {
        int duration = calculatePercentile(duration_window_);
        absl::MutexLock lock(&mu_);
        preemption_time_slice_ = absl::Milliseconds(duration);
        // std::cout << "AdaptPreemptionByDuration: " << preemption_time_slice_
        //           << std::endl;
        count_AdaptPreemptionByDuration.store(0);
    }
}

// convert the task to a CFS task
CfsTask* HybridScheduler::ConvertToCfsTask(ShortQueueTask* task,
                                           const Message& msg) {
    const ghost_msg_payload_task_new* payload =
        static_cast<const ghost_msg_payload_task_new*>(msg.payload());

    CfsTask* cfsTask = new CfsTask(task->gtid, task->status_word.sw_info());
    cfsTask->elapsed_runtime = task->elapsed_runtime;
    cfsTask->Advance(task->seqnum + 1);
    return cfsTask;
}

// task is first time scheduled to CFS
void HybridScheduler::TaskNewToCfs(ShortQueueTask* task, CfsTask* cfsTask,
                                   const Message& msg) {
    const ghost_msg_payload_task_new* payload =
        static_cast<const ghost_msg_payload_task_new*>(msg.payload());
    cpulist_mu_.Lock();
    cfsTask->cpu = SelectTaskRq(cfsTask).id();
    cpulist_mu_.Unlock();
    task->cpu = topology()->cpu(cfsTask->cpu);

    CpuList cpu_affinity = MachineTopology()->EmptyCpuList();
    if (GhostHelper()->SchedGetAffinity(cfsTask->gtid, cpu_affinity) != 0) {
        cpu_affinity = GetLongCPUList();
    }
    cfsTask->cpu_affinity = cpu_affinity;
    cfsTask->nice = payload->nice;
    cfsTask->weight = kNiceToWeight[cfsTask->nice - kMinNice];
    cfsTask->inverse_weight = kNiceToInverseWeight[cfsTask->nice - kMinNice];
    CfsCpuState* cs = cfs_cpu_state_of(cfsTask);

    absl::MutexLock lock(&cs->run_queue.mu_);
    cs->run_queue.mu_.AssertHeld();
    cfsTask->task_state.SetState(CfsTaskState::State::kRunnable);
    cs->run_queue.EnqueueTask(cfsTask);
    task->new_to_cfs.store(false, std::memory_order_relaxed);
}

// handle the CFS task done
void HybridScheduler::HandleTaskDone(CfsTask* task, bool from_switchto) {
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
        CfsCpuState* cs = cfs_cpu_state_of(task);
        cs->run_queue.mu_.AssertHeld();

        // Remove any pending migration on this task.
        cs->migration_queue.DequeueTask(task);

        CfsTaskState::State prev_state = task->task_state.GetState();
        task->task_state.SetState(CfsTaskState::State::kDone);

        if ((prev_state == CfsTaskState::State::kRunning || from_switchto) ||
            prev_state == CfsTaskState::State::kRunnable ||
            prev_state == CfsTaskState::State::kBlocked) {
            if (cs->current != task) {
                // Remove from the rq and free it.
                cs->run_queue.DequeueTask(task);
                cfs_allocator_->FreeTask(task);
                cs->run_queue.UpdateMinVruntime(cs);
            }
        } else {
            DPRINT_CFS(
                1, absl::StrFormat(
                       "TaskDeparted/Dead cases were not exhaustive, got %s",
                       absl::FormatStreamed(CfsTaskState::State(prev_state))));
        }
    }
}

// enqueue the task to the FIFO queue
void ShortQueueRq::Enqueue(ShortQueueTask* task) {
    CHECK_EQ(task->run_state, ShortQueueTaskState::kRunnable);
    task->run_state = ShortQueueTaskState::kQueued;

    if (task->prio_boost) {
        run_queue_.push_front(task);
    } else {
        run_queue_.push_back(task);
    }
}

// dequeue the task from the FIFO queue
ShortQueueTask* ShortQueueRq::Dequeue() {
    if (RunqueueEmpty()) {
        return nullptr;
    }

    ShortQueueTask* task = run_queue_.front();
    CHECK(task->queued());
    task->run_state = ShortQueueTaskState::kRunnable;
    run_queue_.pop_front();
    return task;
}

// remove the task from the FIFO queue
void ShortQueueRq::RemoveFromRunqueue(ShortQueueTask* task) {
    CHECK_EQ(task->run_state, ShortQueueTaskState::kQueued);

    for (int pos = run_queue_.size() - 1; pos >= 0; pos--) {
        if (run_queue_[pos] == task) {
            // Caller is responsible for updating 'run_state' if task is
            // no longer runnable.
            task->run_state = ShortQueueTaskState::kRunnable;
            run_queue_.erase(run_queue_.cbegin() + pos);
            return;
        }
    }
    CHECK(false);
}

// write a function that shows the elements in the ShortQueueRq
void ShortQueueRq::Show() {
    for (int i = 0; i < run_queue_.size(); i++) {
        std::cout << run_queue_[i]->gtid.describe() << std::endl;
    }
}

// the scheduling part of the Hybrid Scheduler
uint32_t failed_count = 0;
void HybridScheduler::ShortQueueSchedule(const StatusWord& agent_sw,
                                         BarrierToken agent_sw_last) {
    const int global_cpu_id = GetGlobalCPUId();
    CpuList short_cpulist = GetShortCPUList();
    CpuList short_available = topology()->EmptyCpuList();
    CpuList short_assigned = topology()->EmptyCpuList();
    CpuList long_cpulist = GetLongCPUList();
    CpuList long_available = topology()->EmptyCpuList();
    CpuList long_assigned = topology()->EmptyCpuList();

    // run the tasks on the short cpu, check the available short cpus
    for (const Cpu& cpu : short_cpulist) {
        CpuState* cs = cpu_state(cpu);

        if (cpu.id() == global_cpu_id) {
            // CHECK_EQ(cs->current, nullptr);
            if (cs->current != nullptr) {
                std::cout << "global_cpu_id: " << global_cpu_id << std::endl;
            }
            cs->current = nullptr;
            continue;
        }

        if (!Available(cpu)) {
            continue;
        }

        if (cs->current &&
            cs->current->elapsed_runtime < GetPreemptionTimeSlice()) {
            cs->current->UpdateRuntime();
            continue;
        }

        short_available.Set(cpu);
    }

    while (!short_available.Empty()) {
        ShortQueueTask* task = short_queue_.Dequeue();
        if (!task) {
            break;
        }

        const Cpu& cpu = short_available.Front();
        CpuState* cs = cpu_state(cpu);

        cs->current = task;

        short_available.Clear(cpu);
        short_assigned.Set(cpu);

        RunRequest* req = enclave()->GetRunRequest(cpu);
        req->Open({.target = task->gtid,
                   .target_barrier = task->seqnum,
                   .commit_flags = COMMIT_AT_TXN_COMMIT});
    }

    if (!short_assigned.Empty()) {
        enclave()->CommitRunRequests(short_assigned);
    }
    for (const Cpu& cpu : short_assigned) {
        CpuState* cs = cpu_state(cpu);
        RunRequest* req = enclave()->GetRunRequest(cpu);
        if (req->succeeded()) {
            // std::cout << "ShortTask " << cs->current->gtid.describe()
            //           << " is on " << cpu.id() << std::endl;
            // log the time when the program was first run
            if (cs->current->has_run == false) {
                std::ofstream outfile;
                outfile.open("project/log/metrics_Hybrid_firecracker.txt",
                             std::ios::app);
                absl::Time now = absl::Now();
                outfile << "FirstRun: " << cs->current->gtid.describe()
                        << " starts in short queue at " << now << " on cpu "
                        << cpu << std::endl;
                outfile.close();
            }
            // change the has_run flag to true
            TaskOnCpu(cs->current, cpu);
        } else {
            absl::Time now = absl::Now();
            GHOST_DPRINT(3, stderr,
                         "Task %s : commit failed on short cpu %d (state=%d)",
                         cs->current->gtid.describe(), cpu.id(), req->state());
            cs->current->prio_boost = true;
            // commit failed, so push 'current' to the front of runqueue
            short_queue_.Enqueue(cs->current);
            // task failed, so clear out the current task
            cs->current = nullptr;
        }
    }

    // run cfs scheduler among long_cpulist
    for (const Cpu& cpu : long_cpulist) {
        CfsCpuState* cs = cfs_cpu_state(cpu);
        cs->id = cpu.id();
        absl::MutexLock lock(&cs->run_queue.mu_);
        cs->run_queue.mu_.AssertHeld();
        cs->run_queue.SetMinGranularity(min_granularity_);
        cs->run_queue.SetLatency(latency_);

        if (!cs->run_queue.IsEmpty() &&
            cs->ConvertToShort.load(std::memory_order_relaxed) == false) {
            long_assigned.Set(cpu);
            // std::cout << " Long list " << cpu.id() << std::endl;
        }
    }

    if (!long_assigned.Empty()) {
        enclave()->CommitRunRequests(long_assigned);
    }

    for (const Cpu& cpu : long_assigned) {
        RunRequest* req = enclave()->GetRunRequest(cpu);
        CfsCpuState* cs = cfs_cpu_state(cpu);
        CfsTask* prev = nullptr;
        cs->run_queue.mu_.Lock();
        CfsTask* next =
            cs->run_queue.PickNextTask(prev, cfs_allocator_.get(), cs);
        cs->run_queue.mu_.Unlock();

        if (!next && idle_load_balancing_) {
            next = NewIdleBalance(cs);
        }

        cs->current = next;

        if (next) {
            DPRINT_CFS(2, absl::StrFormat("[%s]: Picked via PickNextTask",
                                          next->gtid.describe()));

            req->Open({.target = next->gtid,
                       .target_barrier = next->seqnum,
                       .agent_barrier = agent_sw_last,
                       .commit_flags = COMMIT_AT_TXN_COMMIT});

            while (next->status_word.on_cpu()) {
                Pause();
            }

            uint64_t before_runtime = next->status_word.runtime();
            if (req->Commit()) {
                GHOST_DPRINT(3, stderr, "CfsTask %s oncpu %d",
                             next->gtid.describe(), cpu.id());
                uint64_t runtime = next->status_word.runtime() - before_runtime;
                next->vruntime += absl::Nanoseconds(static_cast<uint64_t>(
                    static_cast<absl::uint128>(next->inverse_weight) *
                        runtime >>
                    22));
            } else {
                GHOST_DPRINT(3, stderr, "CfsSchedule: task %s commit failed",
                             next->gtid.describe());
                failed_count++;
                // std::cout << "CFS task failed " << failed_count << std::endl;
                cs->run_queue.PutPrevTask(next);
            }
        } else {
            req->LocalYield(agent_sw_last, RTLA_ON_IDLE);
        }
    }
}

// CFSScheduler
Cpu HybridScheduler::SelectTaskRq(CfsTask* task) {
    static auto begin = long_cpulist_.begin();
    static auto end = long_cpulist_.end();
    static auto next = end;

    if (long_cpulist_.begin() != begin || long_cpulist_.end() != end) {
        begin = long_cpulist_.begin();
        end = long_cpulist_.end();
        next = end;
    }

    if (next == end) {
        next = begin;
    }

    if (next == end) {
        throw std::runtime_error("No CPUs available");
    }

    Cpu currentCpu = *next;
    next++;
    return currentCpu;
}

// CFS put the previous task to the run queue
void HybridScheduler::PutPrevTask() {
    CfsCpuState* cs = &cfs_cpu_states_[MyCpu()];
    cs->run_queue.mu_.AssertHeld();

    CfsTask* task = cs->current;
    cs->current = nullptr;

    task->task_state.SetState(CfsTaskState::State::kRunnable);

    // Task affinity no longer allows this CPU to run the task. We should
    // migrate this task.
    if (!task->cpu_affinity.IsSet(task->cpu)) {
        StartMigrateTask(task);
    } else {  // Otherwise just add the task into this CPU's run queue.
        cs->run_queue.PutPrevTask(task);
    }
}

// migrate the tasks from the migration queue to the run queue
void HybridScheduler::StartMigrateTask(CfsTask* task) {
    CfsCpuState* cs = cfs_cpu_state_of(task);
    cs->run_queue.mu_.AssertHeld();

    cs->run_queue.DequeueTask(task);
    task->task_state.SetOnRq(CfsTaskState::OnRq::kMigrating);
    cs->migration_queue.EnqueueTask(task);
}

// move tasks from run queue to migration queue
void HybridScheduler::StartMigrateTask(CfsCpuState* cs) {
    absl::MutexLock lock(&cs->run_queue.mu_);
    cs->run_queue.mu_.AssertHeld();
    while (!cs->run_queue.IsEmpty()) {
        CfsTask* task = cs->run_queue.LeftmostRqTask();
        if (!task) break;
        cs->run_queue.DequeueTask(task);
        task->task_state.SetOnRq(CfsTaskState::OnRq::kMigrating);
        cs->migration_queue.EnqueueTask(task);
    }
}

// CFS pick the next task
inline CfsTask* HybridScheduler::NewIdleBalance(CfsCpuState* cs) {
    int load_balanced = LoadBalance(cs, CpuIdleType::kCpuNewlyIdle);
    if (load_balanced <= 0) {
        return nullptr;
    }

    absl::MutexLock lock(&cs->run_queue.mu_);
    return cs->run_queue.PickNextTask(nullptr, cfs_allocator_.get(), cs);
}

// CFS attach the tasks
inline void HybridScheduler::AttachTasks(struct LoadBalanceEnv& env) {
    absl::MutexLock l(&env.dst_cs->run_queue.mu_);

    env.dst_cs->run_queue.AttachTasks(env.tasks);
    env.imbalance -= env.tasks.size();
}

// CFS detach the tasks
inline int HybridScheduler::DetachTasks(struct LoadBalanceEnv& env) {
    absl::MutexLock l(&env.src_cs->run_queue.mu_);

    env.src_cs->run_queue.DetachTasks(env.dst_cs, env.imbalance, env.tasks);

    return env.tasks.size();
}

// CFS calculate the imbalance
inline int HybridScheduler::CalculateImbalance(LoadBalanceEnv& env) {
    // Migrate up to half the tasks src_cpu has more then dst_cpu.
    int src_tasks = env.src_cs->run_queue.LocklessSize();
    int dst_tasks = env.dst_cs->run_queue.LocklessSize();
    int excess = src_tasks - dst_tasks;

    env.imbalance = 0;
    if (excess >= 2) {
        env.imbalance =
            std::min(kMaxTasksToLoadBalance, static_cast<size_t>(excess / 2));
        env.tasks.reserve(env.imbalance);
    }

    return env.imbalance;
}

// CFS find the busiest queue
inline int HybridScheduler::FindBusiestQueue() {
    int busiest_runnable_nr = 0;
    int busiest_cpu = 0;
    for (const Cpu& cpu : GetLongCPUList()) {
        int src_cpu_runnable_nr = cfs_cpu_state(cpu)->run_queue.LocklessSize();

        if (src_cpu_runnable_nr <= busiest_runnable_nr) continue;

        busiest_runnable_nr = src_cpu_runnable_nr;
        busiest_cpu = cpu.id();
    }

    return busiest_cpu;
}

// CFS check if we should balance
inline bool HybridScheduler::ShouldWeBalance(LoadBalanceEnv& env) {
    // Allow any newly idle CPU to do the newly idle load balance.
    if (env.idle == CpuIdleType::kCpuNewlyIdle) {
        return env.dst_cs->LocklessRqEmpty();
    }
    CpuList long_cpulist = GetLongCPUList();

    // Load balance runs from the first idle CPU or if there are no idle
    // CPUs then the first CPU in the enclave.
    int dst_cpu = long_cpulist.Front().id();
    for (const Cpu& cpu : long_cpulist) {
        CfsCpuState* dst_cs = cfs_cpu_state(cpu);
        if (dst_cs->LocklessRqEmpty()) {
            dst_cpu = cpu.id();
            break;
        }
    }

    return dst_cpu == MyCpu();
}

// CFS Load Balance method
inline int HybridScheduler::LoadBalance(CfsCpuState* cs,
                                        CpuIdleType idle_type) {
    struct LoadBalanceEnv env;
    int my_cpu = MyCpu();

    env.idle = idle_type;
    env.dst_cs = &cfs_cpu_states_[my_cpu];
    if (!ShouldWeBalance(env)) {
        return 0;
    }

    int busiest_cpu = FindBusiestQueue();
    if (busiest_cpu == my_cpu) {
        return 0;
    }

    env.src_cs = &cfs_cpu_states_[busiest_cpu];
    if (!CalculateImbalance(env)) {
        return 0;
    }

    int moved_tasks_cnt = DetachTasks(env);
    if (moved_tasks_cnt) {
        AttachTasks(env);
    }

    return moved_tasks_cnt;
}

void HybridScheduler::PingCpu(const Cpu& cpu) {
    Agent* agent = enclave()->GetAgent(cpu);
    if (agent) {
        agent->Ping();
    }
}

// migrate the task to another cpu when adapting the cpu list
bool HybridScheduler::Migrate(CfsTask* task, Cpu cpu, BarrierToken seqnum) {
    CfsCpuState* cs = cfs_cpu_state(cpu);
    {
        absl::MutexLock l(&cs->run_queue.mu_);

        GHOST_DPRINT(3, stderr, "Migrating task %s to cpu %d",
                     task->gtid.describe(), cpu.id());
        task->cpu = cpu.id();
        cs->run_queue.EnqueueTask(task);
    }

    PingCpu(cpu);

    return true;
}

// migrate the task to another cpu when adapting the cpu list
void HybridScheduler::MigrateTasks(CfsCpuState* cs) {
    // In MigrateTasks, this agent iterates over the tasks in the migration
    // queue and removes tasks whose migrations succeed. If a task fails to
    // migrate, mostly due to new messages for that task, the task will not
    // be removed from the migration queue and this agent will try to
    // migrate it after the next draining loop.
    if (ABSL_PREDICT_TRUE(cs->migration_queue.IsEmpty())) {
        return;
    }

    cs->migration_queue.DequeueTaskIf([this](const CfsMq::MigrationArg& arg) {
        CfsTask* task = arg.task;

        CHECK_NE(task, nullptr);
        CHECK(task->task_state.OnRqMigrating()) << task->gtid.describe();

        cpulist_mu_.Lock();
        Cpu cpu = SelectTaskRq(task);
        cpulist_mu_.Unlock();
        task->cpu = cpu.id();

        return Migrate(task, cpu, task->seqnum);
    });
}

// pick global cpu for the FIFO cores
bool HybridScheduler::PickNextGlobalCPU(BarrierToken agent_barrier,
                                        const Cpu& this_cpu) {
    Cpu target(Cpu::UninitializedType::kUninitialized);
    Cpu global_cpu = topology()->cpu(GetGlobalCPUId());
    CpuList short_cpulist = GetShortCPUList();

    // mod 1024
    if (iterations_ & 0x3FF) {
        return false;
    }

    for (const Cpu& cpu : short_cpulist) {
        if (cpu.id() == global_cpu.id()) continue;

        if (Available(cpu)) {
            target = cpu;
            goto found;
        }
    }

found:
    if (!target.valid()) return false;

    CHECK(target != this_cpu);

    SetGlobalCPU(target);
    enclave()->GetAgent(target)->Ping();

    return true;
}

std::unique_ptr<HybridScheduler> SingleThreadedHybridScheduler(
    Enclave* enclave, CpuList cpulist, int32_t global_cpu,
    CpuList short_cpulist, absl::Duration preemption_time_slice) {
    auto allocator =
        std::make_shared<SingleThreadMallocTaskAllocator<ShortQueueTask>>();
    auto scheduler = std::make_unique<HybridScheduler>(
        enclave, std::move(cpulist), std::move(allocator), global_cpu,
        std::move(short_cpulist), preemption_time_slice);
    return scheduler;
}

// HybridAgent
uint32_t count = 0;
absl::Time last_count = absl::Now();
static std::atomic<bool> hasLogged(false);  // global atomic variable
void HybridAgent::AgentThread() {
    Channel& global_channel = scheduler_->GetDefaultChannel();
    gtid().assign_name("Agent:" + std::to_string(cpu().id()));
    if (verbose() > 1) {
        printf("Agent tid:=%d\n", gtid().tid());
    }
    SignalReady();
    WaitForEnclaveReady();

    PeriodicEdge debug_out(absl::Seconds(1));

    std::atomic<bool> startAdaptation(false);
    std::atomic<bool> startLog(false);
    std::atomic<bool> canChange(true);
    const int cycleLimit = 500;
    std::atomic<int> cyclesSinceLastChange(0);
    while (!Finished() || !scheduler_->Empty()) {
        BarrierToken agent_barrier = status_word().barrier();
        if (cpu().id() != scheduler_->GetGlobalCPUId()) {
            RunRequest* req = enclave()->GetRunRequest(cpu());

            if (verbose() > 1) {
                printf("Agent %d on cpu: %d Idled.\n", gtid().tid(),
                       cpu().id());
            }
            req->LocalYield(agent_barrier, 0);
        } else {
            if (boosted_priority() &&
                scheduler_->PickNextGlobalCPU(agent_barrier, cpu())) {
                continue;
            }

            Message msg;
            while (!(msg = global_channel.Peek()).empty()) {
                scheduler_->DispatchMessage(msg);
                global_channel.Consume(msg);
            }

            // uint32_t short_cpu_count = scheduler_->GetShortCPUSize();
            // scheduler_->GetSlidingWindowPreemptionTimeSlice(short_cpu_count);

            // uint32_t cpu_count = scheduler_->GetCPUSize();
            // if (canChange) {
            //     if (scheduler_->GetShortQueueSize() > 100) {
            //         startAdaptation.store(true);
            //     }
            //     if (startAdaptation) {
            //         float short_load = scheduler_->GetShortCPULoad();
            //         float long_load = scheduler_->GetLongCPULoad();
            //         if (short_load - long_load > 10) {
            //             scheduler_->AddShortCPUList();
            //         } else if (long_load - short_load > 10) {
            //             scheduler_->AddLongCPUList();
            //         }
            //         cyclesSinceLastChange = 0;
            //         canChange.store(false);
            //     }
            // } else {
            //     cyclesSinceLastChange.fetch_add(1);
            //     if (cyclesSinceLastChange >= cycleLimit * cpu_count) {
            //         canChange.store(true);
            //     }
            // }

            // scheduler_->AdaptPreemptionByDuration();
            // if (scheduler_->GetShortQueueSize() > 0) {
            //     startLog.store(true);
            // }
            // if (startLog && !scheduler_->Empty()) {
            // scheduler_->LogPreemptionTimeSlice();
            // scheduler_->LogCPUList();
            // }
            scheduler_->ShortQueueSchedule(status_word(), agent_barrier);

            if (verbose() && debug_out.Edge()) {
                static const int flags =
                    verbose() > 1 ? Scheduler::kDumpStateEmptyRQ : 0;
                if (scheduler_->debug_runqueue_) {
                    scheduler_->debug_runqueue_ = false;
                    scheduler_->DumpState(cpu(), Scheduler::kDumpAllTasks);
                } else {
                    scheduler_->DumpState(cpu(), flags);
                }
            }
        }
    }
    // log the preemption count and the cpu list
    // if (!hasLogged) {
    //     bool expected = false;
    //     if (hasLogged.compare_exchange_strong(expected, true)) {
    //         std::ofstream outfile;
    //         outfile.open("project/log/preempt_count_SCFS.txt",
    //         std::ios::app); if (!outfile.is_open()) {
    //             std::cerr << "Error: Unable to open output file!" <<
    //             std::endl; return;
    //         }
    //         for (int cpu_id = 0; cpu_id < scheduler_->GetCPUSize(); ++cpu_id)
    //         {
    //             auto it = scheduler_->preempt_count_per_cpu.find(cpu_id);
    //             int preemption_count = 0;
    //             if (it != scheduler_->preempt_count_per_cpu.end()) {
    //                 preemption_count = it->second;
    //             }
    //             outfile << "CPU " << cpu_id << " has " << preemption_count
    //                     << " preemption" << std::endl;
    //         }

    //         for (auto i = scheduler_->preemption_time_slice_list.begin();
    //              i != scheduler_->preemption_time_slice_list.end(); i++) {
    //             outfile << *i << std::endl;
    //         }
    //         outfile.close();

    //         std::ofstream outfile_cpu_list;
    //         outfile_cpu_list.open("project/log/log_cpu_list.txt",
    //                               std::ios::app);
    //         if (!outfile_cpu_list.is_open()) {
    //             std::cerr
    //                 << "Error: Unable to open output file log_cpu_list.txt!"
    //                 << std::endl;
    //             return;
    //         }

    //         for (auto i = scheduler_->short_cpu_num_list.begin();
    //              i != scheduler_->short_cpu_num_list.end(); ++i) {
    //             outfile_cpu_list << *i << std::endl;
    //         }
    //         outfile_cpu_list.close();
    //     }
    // }
}

std::ostream& operator<<(std::ostream& os, const ShortQueueTaskState& state) {
    switch (state) {
        case ShortQueueTaskState::kBlocked:
            return os << "kBlocked";
        case ShortQueueTaskState::kRunnable:
            return os << "kRunnable";
        case ShortQueueTaskState::kQueued:
            return os << "kQueued";
        case ShortQueueTaskState::kOnCpu:
            return os << "kOnCpu";
        case ShortQueueTaskState::kYielding:
            return os << "kYielding";
    }
}

}  // namespace ghost
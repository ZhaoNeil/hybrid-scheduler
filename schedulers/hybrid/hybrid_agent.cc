//  These codes are used to implement the Hybrid Agent class, the backbones
//  of the code are from the Ghost project. The Hybrid Scheduler is a
//  combination of the FIFO and the CFS policies.

#include <sys/mman.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "lib/agent.h"
#include "lib/channel.h"
#include "lib/enclave.h"
#include "schedulers/hybrid/hybrid_scheduler.h"

ABSL_FLAG(std::string, ghost_cpus, "0-1", "cpulist");
ABSL_FLAG(int32_t, globalcpu, -1,
          "Global cpu. If -1, then defaults to the first cpu in <cpus>");
ABSL_FLAG(std::string, shortcpulist, "0-1",
          "cpu only for short queue. If -1, then defaults to the first cpu in "
          "<cpus>");
ABSL_FLAG(std::string, enclave, "", "Connect to preexisting enclave directory");
ABSL_FLAG(absl::Duration, preemption_time_slice, absl::Microseconds(50),
          "ShortQueue preemption time slice");

namespace ghost {

static void ParseAgentConfig(ShortQueueConfig* config) {
    CpuList ghost_cpus =
        MachineTopology()->ParseCpuStr(absl::GetFlag(FLAGS_ghost_cpus));
    CHECK(!ghost_cpus.Empty());

    CpuList short_cpulist =
        MachineTopology()->ParseCpuStr(absl::GetFlag(FLAGS_shortcpulist));
    CHECK(!short_cpulist.Empty());

    CHECK_GT(ghost_cpus.Size(), short_cpulist.Size());

    int globalcpu = absl::GetFlag(FLAGS_globalcpu);
    if (globalcpu < 0) {
        CHECK_EQ(globalcpu, -1);
        globalcpu = short_cpulist.Front().id();
        absl::SetFlag(&FLAGS_globalcpu, globalcpu);
    }
    CHECK(short_cpulist.IsSet(globalcpu));

    Topology* topology = MachineTopology();
    config->topology_ = topology;
    config->cpus_ = ghost_cpus;
    config->short_cpulist_ = short_cpulist;
    config->global_cpu_ = topology->cpu(globalcpu);
    config->preemption_time_slice_ = absl::GetFlag(FLAGS_preemption_time_slice);
}

}  // namespace ghost

int main(int argc, char* argv[]) {
    absl::InitializeSymbolizer(argv[0]);
    absl::ParseCommandLine(argc, argv);

    ghost::ShortQueueConfig config;
    ghost::ParseAgentConfig(&config);

    printf("Initializing...\n");

    auto uap =
        new ghost::AgentProcess<ghost::FullHybridAgent<ghost::LocalEnclave>,
                                ghost::ShortQueueConfig>(config);

    ghost::GhostHelper()->InitCore();
    printf("Initialization complete, ghOSt active.\n");
    fflush(stdout);

    ghost::Notification exit;
    ghost::GhostSignals::AddHandler(SIGINT, [&exit](int) {
        static bool first = true;

        if (first) {
            exit.Notify();
            first = false;
            return false;
        }
        return true;
    });

    ghost::GhostSignals::AddHandler(SIGUSR1, [uap](int) {
        uap->Rpc(ghost::HybridScheduler::kDebugRunqueue);
        return false;
    });

    exit.WaitForNotification();

    delete uap;

    printf("\nDone!\n");

    return 0;
}

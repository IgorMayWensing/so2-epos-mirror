// EPOS CPU Affinity Scheduler Component Implementation

#include <process.h>
#include <clerk.h>

__BEGIN_SYS

TSC::Time_Stamp CPU_Affinity::Statistics::_cpu_time[Traits<Build>::CPUS];               // Accumulated CPU time at each hyperperiod
TSC::Time_Stamp CPU_Affinity::Statistics::_last_dispatch_time[Traits<Build>::CPUS];     // Time Stamp of last dispatch
TSC::Time_Stamp CPU_Affinity::Statistics::_last_activation_time;                        // Global Time Stamp of the last heuristic activation
unsigned int CPU_Affinity::Statistics::_least_used_cpu;                                 // CPU with lowest execution time
unsigned int CPU_Affinity::Statistics::_most_used_cpu;                                  // CPU with highest execution time

bool CPU_Affinity::charge(bool end)
{
    if(end) {
        if(Statistics::_last_activation_time == 0)
            Statistics::_last_activation_time = TSC::time_stamp();

        _statistics.thread_execution_time += TSC::time_stamp() - Statistics::_last_dispatch_time[CPU::id()];
        Statistics::_cpu_time[CPU::id()] += TSC::time_stamp() - Statistics::_last_dispatch_time[CPU::id()];

        unsigned int id_min = ANY;
        unsigned int id_max = ANY;
        if(TSC::time_stamp() - Statistics::_last_activation_time >= 200000) {
            // get min
            unsigned long long aux = ANY;
            for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
                if(aux > Statistics::_cpu_time[i]) {
                    aux = Statistics::_cpu_time[i];
                    id_min = i;
                }
            }
            // get max
            aux = 0;
            for(unsigned int i = 0; i < Traits<Build>::CPUS; i++) {
                if(aux < Statistics::_cpu_time[i]) {
                    aux = Statistics::_cpu_time[i];
                    id_max = i;
                }
            }
        }
        Statistics::_least_used_cpu = id_min;
        Statistics::_most_used_cpu = id_max;
        return true;
    } else {
        return false;
    }
}

bool CPU_Affinity::award(bool end)
{
    if(end) {
        Statistics::_last_dispatch_time[CPU::id()] = TSC::time_stamp();
        // Heuristic application
        unsigned int id_min = Statistics::_least_used_cpu;
        unsigned int id_max = Statistics::_most_used_cpu;
        if(id_min != ANY && id_max != ANY) {
            if(id_max == CPU::id() && id_min != id_max) {
                if((Statistics::_cpu_time[id_max]) >= (Statistics::_cpu_time[id_min] + _statistics.thread_execution_time)) {
                    _statistics.destination_cpu = Statistics::_least_used_cpu;
                    Statistics::_last_activation_time = 0;
                    Statistics::_least_used_cpu = ANY;
                    Statistics::_most_used_cpu = ANY;
                    return true;
                }
            }
        }
        return false;
    } else {
        return false;
    }
}

__END_SYS

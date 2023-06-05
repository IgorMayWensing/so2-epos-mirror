// EPOS Clerk Implementation

#include <clerk.h>

__BEGIN_SYS

// Clerk
bool Clerk<PMU>::_in_use[Traits<Build>::CPUS][CHANNELS];

// System_Monitor
Simple_List<Monitor> Monitor::_monitors[Traits<Build>::CPUS];
volatile bool Monitor::_enable;

void Monitor::run()
{
    if(_enable) { 
        db<Monitor>(TRC) << "Monitor::run()" << endl;
        Simple_List<Monitor> * monitor = &_monitors[CPU::id()];
        for(List::Iterator it = monitor->begin(); it != monitor->end(); it++)
            it->object()->capture();
    }
}

void Monitor::init()
{
    db<Monitor>(TRC) << "Monitor::init()" << endl;

    if(Traits<System>::monitored)
        init_system_monitoring<0>();

#ifdef __PMU_H

    if(Traits<PMU>::enabled && Traits<PMU>::monitored)
        init_pmu_monitoring<0>();

#endif

}

__END_SYS

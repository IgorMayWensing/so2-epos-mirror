// EPOS CPU Scheduler Component Implementation

#include <process.h>
#include <time.h>

__BEGIN_SYS

// The following Scheduling Criteria depend on Alarm, which is not available at scheduler.h
template <typename ... Tn>
FCFS::FCFS(int p, Tn & ... an): Priority((p == IDLE) ? IDLE : Alarm::elapsed()) {}

// Since the definition above is only known to this unit, forcing its instantiation here so it gets emitted in scheduler.o for subsequent linking with other units is necessary.
template FCFS::FCFS<>(int p);

template <typename ... Tn>
LCFS::LCFS(int p, Tn & ... an): Priority((p == IDLE) ? IDLE : (p == MAIN) ? -(unsigned(1) << (sizeof(int) * 8 - 1)) : (unsigned(1) << (sizeof(int) * 8 - 1)) - Alarm::elapsed() - 2) {}

// Since the definition above is only known to this unit, forcing its instantiation here so it gets emitted in scheduler.o for subsequent linking with other units is necessary.
template LCFS::LCFS<>(int p);

__END_SYS
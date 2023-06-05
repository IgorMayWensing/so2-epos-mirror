// EPOS PC IC Mediator (dispatch) Implementation

#include <machine/ic.h>
#include <process.h>

__BEGIN_SYS

void IC::dispatch(unsigned int i)
{
    bool not_spurious = true;
    if((i >= INT_FIRST_HARD) && (i <= INT_LAST_HARD))
        not_spurious = eoi(i);
    if(not_spurious) {

        if((i != INT_SYS_TIMER) || Traits<IC>::hysterically_debugged)
            db<IC>(TRC) << "IC::dispatch(i=" << i << ")" << endl;

        // The code bellow aims to fix an old problem with the network stack (and other interrupt driven subsystems).
        // If the propagation of an interrupt up the stack causes a thread rescheduling (e.g. a TCP segment is delivered to a blocked application)
        // the ISR, even the reentrant ones, might hold resources (e.g. network buffers) indefinitely.
        // Raising the running thread's priority to a ceiling or to a value which allows preemption only by higher priority threads is an old and straightforward
        // solution!

        Thread::Criterion c;
        if((i != IC::INT_SYS_TIMER) && (i != IC::INT_IPI))
            c = Thread::self()->begin_isr(i);
        _int_vector[i](i);
        if((i != IC::INT_SYS_TIMER) && (i != IC::INT_IPI))
            Thread::self()->end_isr(i, c);
    } else {
        if(i != INT_LAST_HARD)
            db<IC>(TRC) << "IC::spurious interrupt (" << i << ")" << endl;
    }
}

__END_SYS

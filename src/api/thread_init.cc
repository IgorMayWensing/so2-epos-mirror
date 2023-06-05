// EPOS Thread Initialization

#include <machine/timer.h>
#include <machine/ic.h>
#include <system.h>
#include <process.h>
#include <clerk.h>

__BEGIN_SYS

extern "C" { void __epos_app_entry(); }

void Thread::init()
{
    db<Init, Thread>(TRC) << "Thread::init()" << endl;

    typedef int (Main)(int argc, char * argv[]);

    System_Info * si = System::info();
    Main * main;

    if(Traits<System>::multitask)
        main = reinterpret_cast<Main *>(si->lm.app_entry);
    else
        // If EPOS is a library, then adjust the application entry point to __epos_app_entry, which will directly call main().
        // In this case, _init will have already been called, before Init_Application to construct MAIN's global objects.
        main = reinterpret_cast<Main *>(__epos_app_entry);
        
    CPU::smp_barrier();

    Criterion::init();

    static volatile bool task_ready = false;

    if(CPU::id() == 0) {
        if(Traits<System>::multitask) {
            Address_Space * as = new (SYSTEM) Address_Space(MMU::current());
            Segment * cs = new (SYSTEM) Segment(Log_Addr(si->lm.app_code), si->lm.app_code_size, Segment::Flags::APPC);
            Segment * ds = new (SYSTEM) Segment(Log_Addr(si->lm.app_data), si->lm.app_data_size, Segment::Flags::APPD);
            Log_Addr code = si->lm.app_code;
            Log_Addr data = si->lm.app_data;
            int argc = static_cast<int>(si->lm.app_extra_size);
            char ** argv = reinterpret_cast<char **>(si->lm.app_extra);
            new (SYSTEM) Task(as, cs, ds, main, code, data, argc, argv);

            if(si->lm.has_ext)
                db<Init>(INF) << "Thread::init: additional data from mkbi at "  << reinterpret_cast<void *>(si->lm.app_extra) << ":" << si->lm.app_extra_size << endl;

            task_ready = true;
        } else {
            // If EPOS is a library, then adjust the application entry point to __epos_app_entry,
            // which will directly call main(). In this case, _init will already have been called,
            // before Init_Application to construct MAIN's global objects.
            new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::MAIN), reinterpret_cast<int (*)()>(main));
        }

        // Idle thread creation does not cause rescheduling (see Thread::constructor_epilogue)
        new (SYSTEM) Thread(Thread::Configuration(Thread::READY, Thread::IDLE), &Thread::idle);
    } else {
        if(Traits<System>::multitask)
            while (!task_ready);

        new (SYSTEM) Thread(Thread::Configuration(Thread::RUNNING, Thread::IDLE), &Thread::idle);
    }

    CPU::smp_barrier();

    // The installation of the scheduler timer handler does not need to be done after the
    // creation of threads, since the constructor won't call reschedule() which won't call
    // dispatch that could call timer->reset()
    // Letting reschedule() happen during thread creation is also harmless, since MAIN is
    // created first and dispatch won't replace it nor by itself neither by IDLE (which
    // has a lower priority)
    if(Criterion::timed && (CPU::id() == 0))
        _timer = new (SYSTEM) Scheduler_Timer(QUANTUM, time_slicer);

    // No more interrupts until we reach init_end
    CPU::int_disable();

    // Install an interrupt handler to receive forced reschedules
    if(smp) {
        if(CPU::id() == 0)
            IC::int_vector(IC::INT_RESCHEDULER, rescheduler);
        IC::enable(IC::INT_RESCHEDULER);
    }

   if(monitored)
        Monitor::init();

    // Transition from CPU-based locking to thread-based locking
    CPU::smp_barrier();
    This_Thread::not_booting();
}

__END_SYS

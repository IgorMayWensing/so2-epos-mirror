// EPOS Scheduler Component Declarations

#ifndef __scheduler_h
#define __scheduler_h

#include <architecture/cpu.h>
#include <architecture/pmu.h>
#include <architecture/tsc.h>
#include <utility/scheduling.h>
#include <utility/math.h>
#include <utility/convert.h>
#include <utility/fann.h>

__BEGIN_SYS

// All scheduling criteria, or disciplines, must define operator int() with
// the semantics of returning the desired order of a given object within the
// scheduling list
class Scheduling_Criterion_Common
{
    friend class _SYS::Thread;
    friend class _SYS::Periodic_Thread;
    friend class _SYS::RT_Thread;
    friend class _SYS::Clerk<System>;         // for _statistics

public:
    // Priorities
    enum : int {
        ISR    = -1000,
        MAIN   = 0,
        HIGH   = 1,
        NORMAL = (unsigned(1) << (sizeof(int) * 8 - 1)) - 3,
        LOW    = (unsigned(1) << (sizeof(int) * 8 - 1)) - 2,
        IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
    };

    // Constructor helpers
    enum : unsigned int {
        SAME        = 0,
        NOW         = 0,
        UNKNOWN     = 0,
        ANY         = -1U
    };

    // Policy types
    enum : int {
        PERIODIC    = HIGH,
        APERIODIC   = NORMAL,
        SPORADIC    = NORMAL
    };

    // Policy traits
    static const bool timed = false;
    static const bool dynamic = false;
    static const bool preemptive = true;
    static const bool collecting = false;
    static const bool charging = false;
    static const bool awarding = false;
    static const bool migrating = false;
    static const bool track_idle = false;
    static const bool task_wide = false;
    static const bool cpu_wide = false;
    static const bool system_wide = false;
    static const unsigned int QUEUES = 1;

    // Runtime Statistics (for policies that don't use any; that´s why its a union)
    union Statistics {
        // Thread Execution Time
        TSC::Time_Stamp thread_execution_time;  // accumulated thread execution time
        TSC::Time_Stamp last_thread_dispatch;   // time stamp of last dispatch

        // Deadline Miss count - Used By Clerk
        Alarm * alarm_times;                    // pointer to RT_Thread private alarm (for monitoring purposes)
        unsigned int finished_jobs;             // number of finished jobs given by the number of times alarm->p() was called for this thread
        unsigned int missed_deadlines;          // number of missed deadlines given by the number of finished jobs (finished_jobs) minus the number of dispatched jobs (alarm_times->times)

        // CPU Execution Time (capture ts)
        static TSC::Time_Stamp _cpu_time[Traits<Build>::CPUS];              // accumulated CPU time in the current hyperperiod for each CPU
        static TSC::Time_Stamp _last_dispatch_time[Traits<Build>::CPUS];    // time Stamp of last dispatch in each CPU
        static TSC::Time_Stamp _last_activation_time;                       // global time stamp of the last heuristic activation
        
        // Migration Auxiliary
        unsigned int destination_cpu;
    };

protected:
    Scheduling_Criterion_Common() { _statistics.destination_cpu = ANY; }

public:
    const Microsecond period() { return 0;}
    void period(const Microsecond & p) {}

    unsigned int queue() const { return 0; }
    void queue(unsigned int q) {}

    bool update() { return false; }

    bool collect(bool end = false) { return false; }
    bool charge(bool end = false) { return true; }
    bool award(bool end = false) { return true; }

    volatile Statistics & statistics() { return _statistics; }

    static void init() {}

protected:
    Statistics _statistics;
};

// Priority (static and dynamic)
class Priority: public Scheduling_Criterion_Common
{
    friend class _SYS::Thread;
    friend class _SYS::Periodic_Thread;
    friend class _SYS::RT_Thread;

public:
    template <typename ... Tn>
    Priority(int p = NORMAL, Tn & ... an): _priority(p) {}

    operator const volatile int() const volatile { return _priority; }

protected:
    volatile int _priority;
};

// Round-Robin
class RR: public Priority
{
public:
    static const bool timed = true;
    static const bool dynamic = false;
    static const bool preemptive = true;

public:
    template <typename ... Tn>
    RR(int p = NORMAL, Tn & ... an): Priority(p) {}
};

// First-Come, First-Served (FIFO)
class FCFS: public Priority
{
public:
    static const bool timed = false;
    static const bool dynamic = false;
    static const bool preemptive = false;

public:
    template <typename ... Tn>
    FCFS(int p = NORMAL, Tn & ... an);
};


// Multicore Algorithms
class Variable_Queue_Scheduler
{
protected:
    Variable_Queue_Scheduler(unsigned int queue): _queue(queue) {};

    const volatile unsigned int & queue() const volatile { return _queue; }
    void queue(unsigned int q) { _queue = q; }

protected:
    volatile unsigned int _queue;
    static volatile unsigned int _next_queue;
};

// Global Round-Robin
class GRR: public RR
{
public:
    static const unsigned int HEADS = Traits<Machine>::CPUS;

public:
    template <typename ... Tn>
    GRR(int p = NORMAL, Tn & ... an): RR(p) {}

    static unsigned int current_head() { return CPU::id(); }
};

// Fixed CPU (fully partitioned)
class Fixed_CPU: public Priority, public Variable_Queue_Scheduler
{
public:
    static const bool timed = true;
    static const bool dynamic = false;
    static const bool preemptive = true;

    static const unsigned int QUEUES = Traits<Machine>::CPUS;

public:
    template <typename ... Tn>
    Fixed_CPU(int p = NORMAL, unsigned int cpu = ANY, Tn & ... an)
    : Priority(p), Variable_Queue_Scheduler(((_priority == IDLE) || (_priority == MAIN)) ? CPU::id() : (cpu != ANY) ? cpu : ++_next_queue %= CPU::cores()) {}

    using Variable_Queue_Scheduler::queue;
    static unsigned int current_queue() { return CPU::id(); }
};

// CPU Affinity
class CPU_Affinity: public Priority, public Variable_Queue_Scheduler
{
    friend class _SYS::Clerk<System>;         // for _statistics

public:
    static const bool timed = true;
    static const bool dynamic = false;
    static const bool preemptive = true;
    static const bool collecting = true;
    static const bool charging = true;
    static const bool awarding = true;
    static const bool heuristic = true;
    static const unsigned int QUEUES = Traits<Machine>::CPUS;

    struct Statistics {
        Statistics(): thread_execution_time(0), destination_cpu(ANY) {}

        TSC::Time_Stamp thread_execution_time;                              // accumulated execution time (i.e. sum of all jobs)
        unsigned int destination_cpu;                                       // for migrations

        Alarm * alarm_times;                                                // reference to Periodic_Thread->_alarm for monitoring purposes
        unsigned int finished_jobs;                                         // number of finished jobs (i.e. number of times Periodic_Thread->_alarm called Periodic_Thread->_semaphor->p() for this thread)
        unsigned int missed_deadlines;                                      // number of missed deadlines (finished_jobs - number of dispatched jobs (i.e. Periodic_Thread->_alarm->_times))

        static TSC::Time_Stamp _cpu_time[Traits<Build>::CPUS];              // accumulated cpu execution time in the current hyperperiod
        static TSC::Time_Stamp _last_dispatch_time[Traits<Build>::CPUS];    // time stamp of last dispatch
        static TSC::Time_Stamp _last_activation_time;                       // time stamp of the last heuristic activation

        static unsigned int _least_used_cpu;                                // cpu with lowest execution time
        static unsigned int _most_used_cpu;                                 // cpu with highest execution time
    };

public:
    template <typename ... Tn>
    CPU_Affinity(int p = NORMAL, unsigned int cpu = ANY, Tn & ... an)
    : Priority(p), Variable_Queue_Scheduler(((_priority == IDLE) || (_priority == MAIN)) ? CPU::id() : (cpu != ANY) ? cpu : ++_next_queue %= CPU::cores()) {}

    bool charge(bool end = false);
    bool award(bool end = false);

    volatile Statistics & statistics() { return _statistics; }

    using Variable_Queue_Scheduler::queue;
    static unsigned int current_queue() { return CPU::id(); }

private:
    Statistics _statistics;
};

// Real-time Algorithms
class Real_Time_Scheduler_Common: public Priority
{
protected:
    Real_Time_Scheduler_Common(int p): Priority(p), _deadline(0), _period(0), _capacity(0) {} // aperiodic
    Real_Time_Scheduler_Common(int i, const Microsecond & d, const Microsecond & p, const Microsecond & c)
    : Priority(i), _deadline(d), _period(p), _capacity(c) {}

public:
    const Microsecond period() { return _period; }
    void period(const Microsecond & p) { _period = p; }

public:
    Microsecond _deadline;
    Microsecond _period;
    Microsecond _capacity;
};

// Rate Monotonic
class RM:public Real_Time_Scheduler_Common
{
public:
    static const bool timed = false;
    static const bool dynamic = false;
    static const bool preemptive = true;

public:
    RM(int p = APERIODIC): Real_Time_Scheduler_Common(p) {}
    RM(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : Real_Time_Scheduler_Common(p ? p : d, d, p, c) {}
};

// Partitioned Rate Monotonic (multicore)
class PRM: public RM, public Variable_Queue_Scheduler
{
public:
    static const unsigned int QUEUES = Traits<Machine>::CPUS;

public:
    PRM(int p = APERIODIC)
: RM(p), Variable_Queue_Scheduler(((_priority == IDLE) || (_priority == MAIN)) ? CPU::id() : 0) {}

    PRM(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : RM(d, p, c, cpu), Variable_Queue_Scheduler((cpu != ANY) ? cpu : ++_next_queue %= CPU::cores()) {}

    using Variable_Queue_Scheduler::queue;
    static unsigned int current_queue() { return CPU::id(); }
};

// Deadline Monotonic
class DM: public Real_Time_Scheduler_Common
{
public:
    static const bool timed = false;
    static const bool dynamic = false;
    static const bool preemptive = true;

public:
    DM(int p = APERIODIC): Real_Time_Scheduler_Common(p) {}
    DM(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : Real_Time_Scheduler_Common(d, d, p, c) {}
};

// Earliest Deadline First
class EDF: public Real_Time_Scheduler_Common
{
public:
    static const bool timed = true;
    static const bool dynamic = true;
    static const bool preemptive = true;

public:
    EDF(int p = APERIODIC): Real_Time_Scheduler_Common(p) {}
    EDF(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY);

    void update();
};

// Global Earliest Deadline First (multicore)
class GEDF: public EDF
{
public:
    static const unsigned int HEADS = Traits<Machine>::CPUS;

public:
    GEDF(int p = APERIODIC): EDF(p) {}
    GEDF(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : EDF(d, p, c, cpu) {}

    unsigned int queue() const { return current_head(); }
    void queue(unsigned int q) {}
    static unsigned int current_head() { return CPU::id(); }
};

// Partitioned Earliest Deadline First (multicore)
class PEDF: public EDF, public Variable_Queue_Scheduler
{
public:
    static const unsigned int QUEUES = Traits<Machine>::CPUS;

public:
    PEDF(int p = APERIODIC)
    : EDF(p), Variable_Queue_Scheduler(((_priority == IDLE) || (_priority == MAIN)) ? CPU::id() : 0) {}

    PEDF(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : EDF(d, p, c, cpu), Variable_Queue_Scheduler((cpu != ANY) ? cpu : ++_next_queue %= CPU::cores()) {}

    using Variable_Queue_Scheduler::queue;
    static unsigned int current_queue() { return CPU::id(); }
};

// Energy-Aware ANN - Partitioned Earliest Deadline First (multicore)
class EA_PEDF: public PEDF
{
    friend class _SYS::Thread;
    friend class _SYS::Periodic_Thread;
    friend class _SYS::RT_Thread;
    friend class _SYS::Clerk<System>;         // for _statistics

public:
    static const bool collecting = true;
    static const bool charging = true;
    static const bool awarding = true;
    static const bool migrating = true;
    static const bool system_wide = true;

    // PMU Monitoring
    static const unsigned int PMU_EVENTS = 6;

    // ANN
    static const unsigned int INPUTS = PMU_EVENTS + 2; // Thread Execution Time and Current CPU Frequency
    static const unsigned int MAX_TRAINS = 8;
    static const unsigned int SAMPLES_PER_TRAIN_LIMIT = 16;
    static constexpr float ANN_DEVIATION_THRESHOLD = 0.02;

    // Voting and DVFS
    static constexpr float SAFETY_MARGIN = 0.05;
    static const unsigned int FREQUENCY_LEVEL = 100 * 1000 * 1000; // 100 MHz

    // Activity Weights
    static const bool TRAIN_WEIGHTS = false;
    static constexpr float LEARNING_RATE = 0.7;

    struct Activity {
        Activity() { reset(); }

        Activity(const Activity &a) {
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                vector[i] = a.vector[i];
            utilization = a.utilization;
        }

        void reset() {
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                vector[i] = 0;
            utilization = 0;
        }

        void operator+=(const Activity & a) {
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                vector[i] += a.vector[i];
            utilization += a.utilization;
        }

        void operator-=(const Activity & a) {
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                vector[i] -= a.vector[i];
            utilization -= a.utilization;
        }

        float sum() {
            float result = 0;
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                result += vector[i];
            return result;
        }

        bool fits(float* _vector) {
            bool result = true;
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                result &= vector[i]+ _vector[i] < (1 * _activity_weights[i]);
            return result;
        }

        bool fits(const Activity & a) {
            bool result = true;
            for(unsigned int i = 0; i < PMU_EVENTS; ++i)
                result &= vector[i]+ a.vector[i] <= (1 * _activity_weights[i]);
            return result;// && (a.utilization + utilization < 1 - Traits<Criterion>::VARIANCE_THRESHOLDS[0]);
        }

        float vector[PMU_EVENTS];
        float utilization;
    };

    struct Statistics {
        Statistics(): thread_execution_time(0), last_thread_dispatch(0), destination_cpu(ANY), output(0) {
            for(unsigned int i = 1; i < Traits<Build>::CPUS; ++i)
                migration_locked[i] = false;

            for(unsigned int i = 0; i < PMU_EVENTS; ++i) {
                input[i] = 0;
                last_pmu_read[i] = 0;
                pmu_accumulated[i] = 0;
            }
            input[6] = 0; // Thread Execution Time
            input[7] = 0; // CPU Frequency
        }

        // Thread Execution Time
        TSC::Time_Stamp thread_execution_time;  // Sum of jobs execution time
        TSC::Time_Stamp last_thread_dispatch;   // The timestamp of the last dispatch
        // On context-switch: execution time += TSC::timestamp() - last_dispatch
        // Migration
        unsigned int destination_cpu; // to help migration
        bool migration_locked[Traits<Build>::CPUS];

        // ANN
        float input[INPUTS];
        float output;

        Activity activity;
        Activity last_activity;

        // Per-Task Monitoring PMU
        PMU::Count pmu_accumulated[PMU_EVENTS];   // accumulated PMU counters during thread execution
        PMU::Count last_pmu_read[PMU_EVENTS];     // the pmu read() when thread entered CPU

        // Dealine Miss count - Used By Clerk<System>
        Alarm * alarm_times;            // Reference to RT_Thread private alarm (for monitoring pourposes)
        unsigned int finished_jobs;     // Number of finished jobs  <=> times alarm->p() has been called for this task
        unsigned int missed_deadlines;  // Number of finished jobs (finished_jobs) - number of dispatched jobs (alarm_times->times)

        // CPU Execution Time
        static TSC::Time_Stamp _cpu_time[Traits<Build>::CPUS];              // accumulated cpu time at each hyperperiod
        static TSC::Time_Stamp _last_hyperperiod[Traits<Build>::CPUS];      // Time Stamp of last hyperperiod
        static TSC::Time_Stamp _hyperperiod;                                // Global Hyperperiod
        static TSC::Time_Stamp _hyperperiod_cpu_time[Traits<Build>::CPUS];  // cpu time in last hyperperiod

        static Activity _activity_cpu[Traits<Build>::CPUS];
        static Activity _last_activity_cpu[Traits<Build>::CPUS];
    };

public:
    EA_PEDF(int p = APERIODIC): PEDF(p) {}

    EA_PEDF(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : PEDF(d, p, c, cpu) {
        if(Statistics::_hyperperiod == 0)
            Statistics::_hyperperiod = Convert::us2count<TSC::Time_Stamp, Time_Base>(TSC::frequency(), p);
        else
            Statistics::_hyperperiod = Math::lcm(Statistics::_hyperperiod, Convert::us2count<TSC::Time_Stamp, Time_Base>(TSC::frequency(), p));
    }

    bool collect(bool end = false);
    bool charge(bool end = false);
    bool award(bool end = false);

    static void init();

    volatile Statistics & statistics() { return _statistics; }

    using PEDF::queue;

private:
    static Hertz _max_clock;
    static Hertz _min_clock;
    static Hertz _last_freq;

    static bool _imbalanced;
    static unsigned int _last_migration_from;
    static unsigned int _last_migration_to;
    static EA_PEDF *_last_migration;
    static EA_PEDF *_last_swap;
    static float _migration_optimization_threshold;
    static unsigned long long _imbalance_threshold;

    static EA_PEDF *_first_criterion[Traits<Build>::CPUS];
    static EA_PEDF *_last_criterion[Traits<Build>::CPUS];

    // Actuation Control
    static bool _cooldown[Traits<Build>::CPUS];                              // not actuate on cooldown
    static bool _decrease_frequency_vote[Traits<Build>::CPUS];               // CPU votes
    static bool _vote_ready[Traits<Build>::CPUS];                            // CPU vote ready
    static bool _to_learn[Traits<Build>::CPUS];                              // online learning
    static float _activity_weights[PMU_EVENTS];    // Migration feature weights

    // ANN Control
    static unsigned int _train_count[Traits<Build>::CPUS];
    static FANN::fann_type *_train_input[Traits<Build>::CPUS][SAMPLES_PER_TRAIN_LIMIT];
    static FANN::fann_type _desired_output[Traits<Build>::CPUS][SAMPLES_PER_TRAIN_LIMIT];
    static struct FANN::fann *_ann[Traits<Build>::CPUS];
    static FANN::fann_type _utilization_prediction[Traits<Build>::CPUS];

    // Collect control
    static unsigned int _collected[Traits<Build>::CPUS];

    static Clerk<PMU>* _bus_access_st_ca53_v8[Traits<Build>::CPUS];
    static Clerk<PMU>* _data_write_stall_st_buffer_full_ca53_v8[Traits<Build>::CPUS];
    static Clerk<PMU>* _immediate_branches_ca[Traits<Build>::CPUS];
    static Clerk<PMU>* _l2d_writeback[Traits<Build>::CPUS];
    static Clerk<PMU>* _cpu_cycles[Traits<Build>::CPUS];
    static Clerk<PMU>* _l1_cache_hits[Traits<Build>::CPUS];

private:
    Statistics _statistics;
    EA_PEDF * _next;
};

// Clustered Earliest Deadline First (multicore)
class CEDF: public EDF, public Variable_Queue_Scheduler
{
public:
    // QUEUES x HEADS must be equal to Traits<Machine>::CPUS
    static const unsigned int HEADS = 2;
    static const unsigned int QUEUES = Traits<Machine>::CPUS / HEADS;

public:
    CEDF(int p = APERIODIC)
    : EDF(p), Variable_Queue_Scheduler(((_priority == IDLE) || (_priority == MAIN)) ? current_queue() : 0) {} // Aperiodic

    CEDF(const Microsecond & d, const Microsecond & p = SAME, const Microsecond & c = UNKNOWN, unsigned int cpu = ANY)
    : EDF(d, p, c, cpu), Variable_Queue_Scheduler((cpu != ANY) ? cpu / HEADS : ++_next_queue %= CPU::cores() / HEADS) {}

    using Variable_Queue_Scheduler::queue;

    static unsigned int current_queue() { return CPU::id() / HEADS; }
    static unsigned int current_head() { return CPU::id() % HEADS; }
};

__END_SYS

__BEGIN_UTIL

// Scheduling Queues
template<typename T>
class Scheduling_Queue<T, GRR>:
public Multihead_Scheduling_List<T> {};

template<typename T>
class Scheduling_Queue<T, Fixed_CPU>:
public Scheduling_Multilist<T> {};

template<typename T>
class Scheduling_Queue<T, CPU_Affinity>:
public Scheduling_Multilist<T> {};

template<typename T>
class Scheduling_Queue<T, PRM>:
public Scheduling_Multilist<T> {};

template<typename T>
class Scheduling_Queue<T, GEDF>:
public Multihead_Scheduling_List<T> {};

template<typename T>
class Scheduling_Queue<T, PEDF>:
public Scheduling_Multilist<T> {};

template<typename T>
class Scheduling_Queue<T, EA_PEDF>:
public Scheduling_Multilist<T> {};

template<typename T>
class Scheduling_Queue<T, CEDF>:
public Multihead_Scheduling_Multilist<T> {};

__END_UTIL

#endif

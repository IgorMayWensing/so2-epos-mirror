// Page coloring test

#include <utility/random.h>
#include <utility/math.h>
#include <machine/display.h>
#include <machine/rtc.h>
#include <time.h>
#include <synchronizer.h>
#include <real-time.h>

using namespace EPOS;

const unsigned int ITERATIONS = 200; // number of times each thread will be executed ~40seg
const unsigned int TEST_REPETITIONS = 1;
const unsigned int THREADS = 17; // number of periodic threads

const unsigned int ARRAY_SIZE = 8 * 1024;
const unsigned int MEMORY_ACCESS = 16384;
const unsigned int WRITE_RATIO = 4;
const unsigned int POLLUTE_BUFFER_SIZE = 16 * 1024;

int pollute_cache(unsigned int repetitions, unsigned int id);
void run(int test);
void collect_wcet(int test);
void print_stats(void);
int job(unsigned int, unsigned int); // function passed to each periodic thread

typedef unsigned int us;

// 17 threads, total utilization of 5.972483, 8 processors
static struct Task_Set {
    int (*f)(unsigned int, unsigned int);
    us p;
    us d;
    us c;
    us affinity;
} set[THREADS] = {
                  // period, deadline, execution time, cpu (partitioned)
                  {&job,  50000,  50000,  32329, 0},
                  {&job,  50000,  50000,   5260, 1},
                  {&job,  50000,  50000,  12295, 2},
                  {&job, 200000, 200000,  62727, 3},
                  {&job, 100000, 100000,  49286, 4},
                  {&job, 200000, 200000,  48083, 5},
                  {&job, 200000, 200000,  22563, 6},
                  {&job, 100000, 100000,  17871, 7},
                  {&job,  25000,  25000,  15211, 1},
                  {&job, 200000, 200000, 129422, 6},
                  {&job, 200000, 200000,  52910, 7},
                  {&job, 100000, 100000,  14359, 5},
                  {&job,  25000,  25000,  14812, 2},
                  {&job,  50000,  50000,  33790, 3},
                  {&job,  25000,  25000,   7064, 5},
                  {&job, 100000, 100000,  20795, 7},
                  {&pollute_cache, 200000, 200000, 42753, 4} //lowest_priority_task*/
};
unsigned int lowest_priority_task = 16;

typedef struct {
    /*us mean[TEST_REPETITIONS];
    us var[TEST_REPETITIONS];
    us wcet[TEST_REPETITIONS];*/
    us * mean;
    us * var;
    us * wcet;
} wcet_stats;

Display d;
OStream cout;
Clock clock;

volatile wcet_stats * stats;
us exec_time;
static const bool same_color = false;
volatile us * wcet[THREADS];
Periodic_Thread * threads[THREADS]; // periodic threads that will be created

int main()
{
    d.clear();
    cout << "Page coloring test!" << endl;
    exec_time = 0;

    stats = new wcet_stats[THREADS];

    for(unsigned int i = 0; i < THREADS; i++) {
        wcet[i] = new us[ITERATIONS];
        stats[i].mean = new us[TEST_REPETITIONS];
        stats[i].var = new us[TEST_REPETITIONS];
        stats[i].wcet = new us[TEST_REPETITIONS];
    }

    for(unsigned int i = 0; i < TEST_REPETITIONS; i++) {
        cout << "Starting test " << i << endl;
        run(i);
    }

    cout << "SDN WFD done!" << endl;
    cout << "Worst-case exec time = " << exec_time / 1000000 << " seconds!" << endl;

    print_stats();

    for(unsigned int i = 0; i < THREADS; i++)
        delete wcet[i];
}

void run(int test)
{
    TSC_Chronometer chrono;

    for(unsigned int i = 0; i <  THREADS; i++)
        for(unsigned int j = 0; j < ITERATIONS; j++)
            wcet[i][j] = 0;

    for(unsigned int i = 0; i < THREADS; i++) {
        cout << "T[" << i << "] p=" << set[i].p << " d=" << set[i].d << " c=" << set[i].c << " a=" << set[i].affinity << endl;
        if(i == lowest_priority_task)
            // p,d,c,act,t,cpu
            threads[i] = new Periodic_Thread(RTConf(us(set[i].p), us(set[i].d), us(set[i].c), 0, ITERATIONS, set[i].affinity, Thread::READY, Thread::Criterion(us(set[i].p), us(set[i].d), us(set[i].c), set[i].affinity), Color(i + 1)),
                                             set[i].f, (unsigned int)((set[i].c / 1730) * 10), i);
        else
            threads[i] = new Periodic_Thread(RTConf(us(set[i].p), us(set[i].d), us(set[i].c), 0, ITERATIONS, set[i].affinity, Thread::READY, Thread::Criterion(us(set[i].p), us(set[i].d), us(set[i].c), set[i].affinity), Color(i + 1)),
                                             set[i].f, (i == 12) ? (unsigned int)(set[i].c / 540) : (unsigned int)((set[i].c / 540) * 3), i);
    }

    chrono.start();

    for(unsigned int i = 0; i <  THREADS; i++)
        threads[i]->join();

    chrono.stop();

    collect_wcet(test);

    for(unsigned int i = 0; i <  THREADS; i++)
        delete threads[i];

    cout << "Page coloring test " << test << " done in " << chrono.read() / 1000000 << " seconds!\n";
}

void collect_wcet(int test)
{
    for(unsigned int i = 0; i < THREADS; i++) {
        us wc, m, var;
        wc = Math::largest(wcet[i], ITERATIONS);
        m = Math::mean(wcet[i], ITERATIONS);
        var = Math::variance(const_cast<us *&>(wcet[i]), ITERATIONS, m);
        stats[i].mean[test] = m;
        stats[i].wcet[test] = wc;
        stats[i].var[test] = var;
    }

    cout << "m = " << stats[0].mean[test] << " wc = " << stats[0].wcet[test] << " var = " << stats[0].var[test] << "\n";
}

void print_stats(void)
{
    for(unsigned int i = 0; i < THREADS; i++) {
        us wc, m, var, wc_m, wc_var;
        if(TEST_REPETITIONS > 1) {
            wc = Math::largest(stats[i].wcet, TEST_REPETITIONS);
            wc_m = Math::mean(stats[i].wcet, TEST_REPETITIONS);
            wc_var = Math::variance(stats[i].wcet, TEST_REPETITIONS, wc_m);
            m = Math::mean(stats[i].mean, TEST_REPETITIONS);
            var = Math::mean(stats[i].var, TEST_REPETITIONS);
        } else {
            wc = stats[i].wcet[0];
            wc_m = stats[i].wcet[0];
            wc_var = 0;
            m = stats[i].mean[0];
            var = 0;
        }

        cout << "Thread " << i << " wc = " << wc << " m = " << m << " var = " << var << " wc m = " << wc_m << " wc var = " << wc_var << "\n";
    }
}

int pollute_cache(unsigned int repetitions, unsigned int id)
{
    int sum = 0;
    TSC_Chronometer c;
    Random * rand;
    int *pollute_buffer;

    if(same_color)
        rand = new (COLOR_2) Random();
    else
        rand = new Random();

    rand->seed(clock.now() + id);

    if(same_color)
        pollute_buffer = new (COLOR_2) int[POLLUTE_BUFFER_SIZE];
    else
        pollute_buffer = new int[POLLUTE_BUFFER_SIZE];

    for(unsigned int i = 0; i <  ITERATIONS; i++) {
        Periodic_Thread::wait_next();

        c.start();

        for(unsigned int j = 0; j < repetitions; j++) {
            for(unsigned int k = (rand->random() % (POLLUTE_BUFFER_SIZE - 1) ) % 1000; k < POLLUTE_BUFFER_SIZE; k += 64) {
                pollute_buffer[k] = j % 64;
                sum += pollute_buffer[k];
            }
        }

        c.stop();

        wcet[id][i] = c.read();

        //if(wcet[id] < c.read())
        //  wcet[id] = c.read();
        c.reset();
    }

    delete rand;
    delete pollute_buffer;

    //cout << "Thread " << id << " done\n";
    return sum;
}

int job(unsigned int repetitions, unsigned int id)
{
    int sum = 0;
    TSC_Chronometer c;
    Random * rand;
    int *array;

    if(same_color)
        rand = new (COLOR_2) Random();
    else
        rand = new Random();

    rand->seed(clock.now() + id);

    if(same_color)
        array = new (COLOR_2) int[ARRAY_SIZE];
    else
        array = new int[ARRAY_SIZE];

    for(unsigned int i = 0; i <  ITERATIONS; i++) {
        Periodic_Thread::wait_next();
        c.reset();

        //if(id == 0)
        //    cout << "Thread " << id << " ite = " << i << " start\n";

        c.start();

        for(unsigned int j = 0; j < repetitions; j++) {
            for(unsigned int k = 0; k < MEMORY_ACCESS; k++) {
                int pos = rand->random() % (ARRAY_SIZE - 1);
                sum += array[pos];
                if((k % WRITE_RATIO) == 0)
                    array[pos] = k + j;
            }
        }

        c.stop();

        wcet[id][i] = c.read();

        if(id == 0)
            cout << "Thread " << id << " ite = " << i << " finished in " << c.read() << " wcet[][]=" << wcet[id][i] << "\n";

        //if(wcet[id] < c.read())
        //  wcet[id] = c.read();
        c.reset();

    }

    delete rand;
    delete array;

    return sum;
}




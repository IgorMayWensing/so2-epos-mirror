#include <utility/ostream.h>
#include <time.h>
#include <process.h>
#include <clerk.h>
#include <real-time.h>
#include "disparity.h"
#include "bandwidth.h"
#include "cpu_hungry.h"

using namespace EPOS;

OStream cout;

typedef TSC::Time_Stamp Time_Stamp;

// Configuration
const unsigned int TEST_LENGTH            = 1; // in seconds // emulating on QEMU takes too long
const bool MEASURE_TIME = false;
// To be measured
const float MEMORY_IT_LENGHT              = 2.5;    // 0.4 in microseconds
const unsigned int CPU_IT_LENGHT          = 8;      // in microseconds
const unsigned int MIDTERM_IT_LENGHT      = 70000;  // in microseconds

// Simple task-set, just to monitor a variety of stress levels (varying memory/cpu/mix usage)
/* 
* T \ C | 1       | 2       | 3       | 4       | 5       | 6       | 7       | 8
*  0 -> | 20      | 15      | 30      | 5       | 35      | 10      | 25      | 40
*  1 -> | 20      | 35      | 10      | 5       | 15      | 30      | 30      | 25
*  2 -> | 18      | 28      | 14      | 17.5    | 35      | 24      | 23.3    | 17.5
*  T -> | 58      | 78      | 54      | 27.5    | 85      | 64      | 78.3    | 82.5
*/

const unsigned int THREAD_NUM             = 8; //12;
unsigned int TS_CONF[][5] = {
    //PERIOD,DEADLINE,WCET,CPU,TASK
    // {100000 ,100000 ,20000 ,1,0},
    // {200000 ,200000 ,30000 ,2,0},
    // {100000 ,100000 ,30000 ,3,0},
    // {200000 ,200000 ,10000 ,4,0},
    // {200000 ,200000 ,70000 ,5,0},
    // {100000 ,100000 ,10000 ,6,0},
    // {200000 ,200000 ,50000 ,7,0},
    // {100000 ,100000 ,40000 ,8,0},
    {100000 ,100000 ,20000 ,1,1},
    {200000 ,200000 ,70000 ,2,1},
    {100000 ,100000 ,10000 ,3,1},
    {200000 ,200000 ,10000 ,4,1},
    {200000 ,200000 ,30000 ,5,1},
    {100000 ,100000 ,30000 ,6,1},
    {100000 ,100000 ,30000 ,7,1},
    {200000 ,200000 ,50000 ,8,1},
    {1000000,1000000,140000,1,2},
    {1000000,1000000,280000,2,2},
    {1500000,1500000,210000,3,2},
    {2000000,2000000,350000,4,2},
    {1000000,1000000,350000,5,2},
    {1000000,1000000,240000,6,2},
    {1500000,1500000,350000,7,2},
    {2000000,2000000,350000,8,2}
};

unsigned int *g_mem_ptr[Traits<Build>::CPUS];           /* unsigned inter to allocated memory region */
extern const unsigned int G_MEM_SIZE;

inline Time_Stamp get_time() {
    return (Traits<Build>::MODEL == Traits<Build>::Raspberry_Pi3 ? TSC::time_stamp() : TSC::time_stamp() / (TSC::frequency()/1000000));
}

unsigned int iter_per_job[THREAD_NUM];
unsigned int jobs[THREAD_NUM];

unsigned int time_job[THREAD_NUM][20];
Time_Stamp diff[THREAD_NUM];
Time_Stamp wcet[THREAD_NUM];
volatile bool locked = true;

int run_func(unsigned int id, unsigned int func) {
    while(locked);
    //cout << "t[" << id << "]" << endl;

    Time_Stamp init = 0;
    Time_Stamp end = 0;

    /*
    unsigned int iters = 0;
    // */

    unsigned int my_iter_per_job = iter_per_job[id];
    unsigned int my_jobs = jobs[id];

    unsigned int iterations;
    unsigned int ret = 0;
    unsigned int cpu;

    for(unsigned int job = 0; job < my_jobs; job++){
        Periodic_Thread::wait_next();
        cpu = CPU::id();
        init = get_time();

        for(iterations = 0; iterations < my_iter_per_job; iterations++) {
            /*
            do { // remove when not measuring
                iters++;
            //*/
                switch(func) {
                   case 0:
                        ret += bandwidth(g_mem_ptr[cpu]);
                        //ret += bandwidth(g_mem_ptr[cpu]);
                       break;
                   case 1:
                        ret += cpu_hungry();
                       break;
                   default:
                       ret += disparity();
                       break;
                }
            /* Remove when not measuring
                end = get_time();
            } while (end - init < TS_CONF[id][2]);
            //cout << "t=" << id << ",i=" << job << endl; // alive test
            // */
        }
        end = get_time();
        diff[id] += end - init;
        if ((end - init)/iterations > wcet[id]){
            wcet[id] = (end - init)/iterations;
        }
        //time_job[id][job] = end - init; // Remove when not measuring
    }
    /*
    if (MEASURE_TIME) {
        if (id > 15)
            iter_per_job[id] = iters/5;
        else
            iter_per_job[id] = iters/20;
    }
    */
    return ret;
}

void calc_iter_per_job() {
    for (unsigned int i = 0; i < THREAD_NUM; i++)
    {
        if (MEASURE_TIME) {
            jobs[i] = 20;
            if (TS_CONF[i][4] == 2)
                jobs[i] = 5;
            iter_per_job[i] = 1;
        } else {
            //if (i == 19) {
            //    iter_per_job[i] = TS_CONF[i][2]/35000;
            //} else {
                switch(TS_CONF[i][4]) { // [4] -> TASK
                                        // [2] ->  WCET
                    case 0:
                        cout << "0 => " << TS_CONF[i][2] << "*" << MEMORY_IT_LENGHT << " = " << TS_CONF[i][2] * MEMORY_IT_LENGHT <<endl; 
                        iter_per_job[i] = TS_CONF[i][2] * MEMORY_IT_LENGHT; // inverse relation
                        break;
                    case 1:
                        iter_per_job[i] = TS_CONF[i][2]/CPU_IT_LENGHT;
                        break;
                    default:
                        iter_per_job[i] = TS_CONF[i][2]/MIDTERM_IT_LENGHT;
                        break;
            //    }
            }
            if(iter_per_job[i] < 1) {
                iter_per_job[i] = 1;
            }
            jobs[i] = TEST_LENGTH*1000000/TS_CONF[i][0]; // [0] -> Period == Deadline
        }
        cout <<"jobs[" << i << "]: " << jobs[i] << ", Iters :" << iter_per_job[i] << endl;
    }
}

int main() // TODO: Add clock change in middle of execution (reschedule probably)
{
    cout << "Begin Main" << endl;
    calc_iter_per_job();

    Thread* threads[THREAD_NUM];

    // SETUP BandWidth
    for (unsigned int i = 0; i < Traits<Build>::CPUS; ++i)
    {
       g_mem_ptr[i] = new unsigned int[G_MEM_SIZE/sizeof(unsigned int)];
       for (unsigned int j = 0; j < G_MEM_SIZE / sizeof(unsigned int); j++) {
            g_mem_ptr[i][j] = j + j*i;
       }
    }

    Monitor::enable_captures();
    Time_Stamp tsc0 = get_time();

    for (unsigned int i = 0; i < THREAD_NUM; i++)
    {
        // RTConf(Period, deadline, capacity, activation, times, cpu, state, criterion), func, par1, par2
        // PEDF == Criterion(d, p, c, cpu)
        threads[i] = new Periodic_Thread(RTConf(TS_CONF[i][0], 0, TS_CONF[i][2], 0, jobs[i], (TS_CONF[i][3]-1), Thread::READY,
            Thread::Criterion(TS_CONF[i][0], TS_CONF[i][1],
            TS_CONF[i][2], (TS_CONF[i][3]-1))), &run_func, i, TS_CONF[i][4]);
    }

    cout << "All TASKs created"<< endl;
    locked = false;

    for (unsigned int i = 0; i < THREAD_NUM; ++i)
    {
        threads[i]->join();
    }

    Monitor::disable_captures();

    Time_Stamp times = get_time() - tsc0;

    //printing thread ids
    cout << "Returned to application main!" << endl;

    cout << "Elapsed = " << times << endl;
    cout << "Threads=" << THREAD_NUM << endl;

    cout << "-----------------------------------------------------" << endl;
    cout << "...............Threads Timing Behavior..............." << endl;
    cout << "-----------------------------------------------------" << endl;
    for (unsigned int i = 0; i < THREAD_NUM; ++i)
    {
        cout<<"t"<<i<<": "<<reinterpret_cast<unsigned int>(threads[i])<<endl;
        cout << "average execution time [" << i << "]: " << diff[i]/(jobs[i]*iter_per_job[i]) << endl;
        cout << "iter_per_job=" << iter_per_job[i] << "; ";
        cout << "WCET [" << i << "]: " << wcet[i] << endl;
        if(MEASURE_TIME) {
            cout << "TIME_JOB:[";
            for (int j = 0; j < 20; ++j)
            {
                cout << "[" << j << "]" << time_job[i][j] << ",";
            }
            cout << "]" << endl;
        }
    }

    cout << "Threads_end" << endl;
    cout << "ID:" << reinterpret_cast<volatile unsigned int>(Thread::self());
    for (unsigned int i = 0; i < THREAD_NUM; ++i)
    {
        cout << "," << reinterpret_cast<volatile unsigned int>(threads[i]);
    }
    cout << "\n";

    return 0;
}

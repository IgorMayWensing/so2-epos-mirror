#include <utility/ostream.h>
#include <process.h>
#include <clerk.h>
#include <real-time.h>
#include <time.h>
#include <machine/machine.h>
#include <utility/random.h>

using namespace EPOS;

OStream cout;

inline TSC::Time_Stamp get_time() {
    return TSC::time_stamp();
}

void run_func() {
    unsigned int cpu = CPU::id();
    TSC::Time_Stamp t0 = 0;

    if (cpu == 1)
        t0 = get_time();

    for (int i = 0; i < 250; ++i)
    {
        Random * rand;
        rand->seed(0);
        int fib = 1;
        int temp = 1;
        int prev = 1;
        fib = 1+rand->random()%3;
        //fib = 1;
        prev = 1;
        for (int j = 1; j < 350000; j++) {
            temp = prev+fib;
            prev = fib;
            fib = temp;
        }
    }

    if (cpu == 1)
        cout << "CPU1@" << CPU::clock() << "Hz, exec_time="<< get_time() - t0 << endl;
}

Thread * threads[6];

int freq_control() {
    int iters = (15*1000000)/1000000; // each 2 hyp
    int count_dvfs = 1;
    for (int i = 0; i < iters; ++i)
    {
        if (!(i % 2) && i > 0) {
            CPU::clock(1200000000 - (count_dvfs % 7)*100000000);
            count_dvfs++;
        }
        Delay(1000000);
    }
    return iters;
}

int main()
{
    cout << "Hello world!" << endl;
    cout << "clock["<< CPU::id() <<"]" << CPU::clock() << endl;
    Hertz new_clock = 1200000000;
    cout << "    clock change to" << new_clock << "Hz" << endl;
    CPU::clock(new_clock);
    cout << "    clock now is = " << CPU::clock() << ",max=" << CPU::max_clock() << endl;

    threads[0] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 1);
    threads[1] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 1);
    threads[2] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 2);
    threads[3] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 2);
    threads[4] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 3);
    threads[5] = new RT_Thread(&run_func, 500000, 500000, 100000, 10000, 30, 3);

    Thread * freq = new Thread(&freq_control);

    for (int i = 0; i < 6; ++i)
    {
        threads[i]->join();
        cout << "T["<< i << "]" << endl;
    }

    freq->join();

    Random * rand;
    rand->seed(0);


    // This was added as a still unkown solution that make Raspberry end its execution...

    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    cout << "Goodbey World" << 1+rand->random()%3 << endl;
    return 0;
}

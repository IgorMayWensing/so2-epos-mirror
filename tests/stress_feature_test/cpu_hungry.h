#ifndef __CPU_HUNGRY__
#define __CPU_HUNGRY__

//method that configures iterative fibonacci execution
int iterative_fib () {
    //float ret = 1.33;
    int fib = 1;
    int temp = 1;
    int prev = 1;
    fib = 1;
    prev = 1;
    for (int j = 1; j < 10000; j++) {
        temp = prev+fib;
        prev = fib;
        fib = temp;
    }
    return fib;
    //return int(ret);
}

//fibonacci recursive method
int fib(int pos) {
    if (pos == 1 || pos == 0) {
        return 1;
    } else {
        return (fib(pos - 1) + fib(pos - 2));
    }

}

//method that runs memcpy method
// int test(int id) {
//     Random * rand;
//     float ret = 1.33;
//     int max = (int) ((threads_parameters[id][2]/BASE_MEMCPY_T));
//     for (int i = 0; i < iterations[id]; i++) {
//         Periodic_Thread::wait_next();
//         rand->seed(clock.now());
//         for (int x = 0; x < max; x++) {
//             memcpy(reinterpret_cast<void *>(&vectors[(x*3)%3]),reinterpret_cast<void *>(&vectors[rand->random() % Machine::n_cpus()]),sizeof(vectors[0]));
//         }
//     }
//     return int(ret);
// }


inline int cpu_hungry() {
    return (int) (1.33 * iterative_fib());
}

#endif
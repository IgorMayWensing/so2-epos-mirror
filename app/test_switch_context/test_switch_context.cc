#include <utility/ostream.h>
#include <machine.h>
#include <time.h>
#include <synchronizer.h>
#include <process.h>

using namespace EPOS;

OStream cout;

const int iterations = 1000*1000*10;


int consumer()
{
    int out = 0;
    for(int i = 0; i < iterations; i++) {
        out += 1;
    }

    return 0;
}


int main()
{
    
    cout << "Test Switch Context!" << endl;
    
    Thread * cons = new Thread(&consumer);
    cons->join();
    cout << "Thread 0 has finished!" << endl;
    
    Thread * cons1 = new Thread(&consumer);
    cons1->join();
    cout << "Thread 1 has finished!" << endl;
    
    Thread * cons2 = new Thread(&consumer);
    cons2->join();
    cout << "Thread 2 has finished!" << endl;
    
    Thread * cons3 = new Thread(&consumer);
    cons3->join();
    cout << "Thread 3 has finished!" << endl;
    
    cout << "Bye!" << endl;
    
    return 0;
}

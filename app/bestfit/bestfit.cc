#include <utility/ostream.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Test Best-Fit alloc:" << endl;

    // allocate two blocks of memory
    char *p1 = new char[1024];
     cout << "Address of p1 = " << (void *)p1 << endl;
    char *p2 = new char[64];
     cout << "Address of p2 = " << (void *)p2 << endl;
    char *p3 = new char[9512];
     cout << "Address of p3 = " << (void *)p3 << endl;


    // free the blocks of memory
    delete [] p1;
    delete [] p2;
    delete [] p3;

    // allocate a new block of memory with size 1024
    char *newp1 = new char[1024];
    cout << "Address of newp1 = " << (void *)newp1 << endl;



    // free the block of memory
    delete [] newp1;

    cout << "End test" << endl;
    return 0;
}


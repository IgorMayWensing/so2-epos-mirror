#include <utility/ostream.h>
#include <utility/elf.h>
#include <architecture.h>
#include <system.h>
#include <time.h>
#include <memory.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Writer beginned" << endl;
    
    unsigned int port = 1;
    Shared_Segment * sseg = new Shared_Segment(port, 1024, MMU::Flags::UDATA);
    
    CPU::Phy_Addr phy_addr = sseg->phy_address();
    cout << "phy_addr_writer=" << phy_addr << endl;
    
    CPU::Log_Addr sseg_log_addr = Task::active()->address_space()->attach(reinterpret_cast<Segment *>(sseg));
    
    cout << "sseg_log_addr_writer=" << sseg_log_addr << endl;
    
    long unsigned int * a = sseg_log_addr;    
    *a = 12345;
    
    cout << "writer_a: " << *a << endl;
    
    Alarm::delay(500000);
    
    char * letters = reinterpret_cast<char *>(a + sizeof(long unsigned int));
    cout << "letters:" << endl;
    for(int i=0;i<26;i++){
        cout << letters[i];
    }
    cout << endl;
    
    cout << "Writer is finishing" << endl;
    Task::active()->address_space()->detach(reinterpret_cast<Segment *>(sseg));
    delete sseg; 
    
    return 0;
}

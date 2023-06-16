#include <utility/ostream.h>
#include <utility/elf.h>
#include <architecture.h>
#include <system.h>
#include <time.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Reader beginned" << endl;
    
    Alarm::delay(100000);
    unsigned int port = 1;
    Shared_Segment * sseg = new Shared_Segment(port, 1024, MMU::Flags::UDATA);
    
    CPU::Phy_Addr phy_addr = sseg->phy_address();
    cout << "phy_addr_reader=" << phy_addr << endl;
    
    CPU::Log_Addr sseg_log_addr = Task::active()->address_space()->attach(reinterpret_cast<Segment *>(sseg));
    
    cout << "sseg_log_addr_reader=" << sseg_log_addr << endl;
    
    long unsigned int * a = sseg_log_addr;    
  
    cout << "reader_a: " << *a << endl;
    
    char * letters = reinterpret_cast<char *>(a + sizeof(long unsigned int));
    char all_letters[50] = "qwertyuiopasdfghjklzxcvbnm";
    for(int i=0;i<26;i++){
        letters[i] = all_letters[i];
    }    
    
    cout << "reader is finishing" << endl;
    Task::active()->address_space()->detach(reinterpret_cast<Segment *>(sseg));
    delete sseg;
    
    
    return 0;
}

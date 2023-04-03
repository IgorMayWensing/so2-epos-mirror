#include <utility/ostream.h>
#include <machine/riscv/riscv_spi.h>

using namespace EPOS;

SPI spi(125000, 2, 1, 8); //mudar os args para ficar de acordo
//    SPI(unsigned int clock, unsigned int protocol, unsigned int mode, unsigned int data_bits) {
OStream cout;
int main()
{

cout<<"START" <<endl;

   int ie = spi.get_ie();
     int ip = spi.get_ip();


     cout << "ie pre-writing: " << ie << endl;
     cout << "ip pre-writing: " << ip << endl;

     const char *data;
     unsigned int size;
     data = "a";
     size = 2; // Length of "a" (1) + 1 for the null terminator
     spi.write(data, size);
    
    //spi.put('a');

      ie = spi.get_ie();
      ip = spi.get_ip();

     cout << "ie post-writing: " << ie << endl;
     cout << "ip post-writing: " << ip << endl;
    return 0;
}

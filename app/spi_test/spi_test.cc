#include <utility/ostream.h>
#include <machine/riscv/riscv_spi.h>

using namespace EPOS;

SPI spi(125000, 1, 0, 0); //mudar os args para ficar de acordo
//    SPI(unsigned int clock, unsigned int protocol, unsigned int mode, unsigned int data_bits) {
OStream cout;
int main()
{
    cout << "Inicio do teste" << endl;
    const char *data;
    char read[7];
    unsigned int size;

    data = "Nelson";

    size = 7; // Length of "Nelson" (6) + 1 for the null terminator
    
    cout << "Antes do write" << endl;
    spi.write(data, size);

    cout << "Entre as funcs" << endl;

    spi.read(read, size);
    cout << "Depois do read" << endl;
    cout << "Read: " <<read << endl;
    return 0;
}

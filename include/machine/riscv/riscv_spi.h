// EPOS RISC-V SPI Mediator Declarations

#ifndef __riscv_spi_h
#define __riscv_spi_h

#include <architecture/cpu.h>
#include <machine/spi.h>
#include <system/memory_map.h>

__BEGIN_SYS

class SPI
{
private:

    typedef CPU::Reg8 Reg8;
    typedef CPU::Reg32 Reg32;

    // SPI registers offsets from SPI_BASE
    enum {
        // Define the register offsets for the SPI control registers
        SCKDIV   = 0x00, //Serial clock divisor
        SCKMODE  = 0x04, //Serial clock mode
        CSID     = 0x10, //Chip select ID
        CSDEF    = 0x14, //Chip select default
        CSMODE   = 0x18, //Chip select mode
        DELAY0   = 0x28, //Delay control 0
        DELAY1   = 0x2C, //Delay control 1
        FMT      = 0x40, //Frame format
        TXDATA   = 0x48, //Tx FIFO Data
        RXDATA   = 0x4C, //Rx FIFO data
        TXMARK   = 0x50, //Tx FIFO watermark
        RXMARK   = 0x54, //Rx FIFO watermark
        FCTRL    = 0x60, //SPI flash interface control*
        FFMT     = 0x64, //SPI flash instruction format*
        IE       = 0x70, //SPI interrupt enable
        IP       = 0x74, //SPI interrupt pending   
    };

    // Useful bits from multiple registers
    enum {
        TXDATA_FULL=    1 << 31,   // TXDATA, TX FIFO full
        FMT_LEN    =   15 << 16,
        EMPTY   =    1 << 31,   // RXDATA, RX FIFO empty
        // DATA    = 0xff << 0,
        // TXEN    =    1 <<  0,   // TXCTRL, TX enable
        // NSTOP   =    1 <<  1,   // TXCTRL, stop bits (0 -> 1 or 1 -> 2)
        // TXCNT   =    7 << 16,   // TXCTRL, TX interrupt threshold (RXWM = (len(FIFO) < TXCNT))
        // RXEN    =    1 <<  0,   // RXCTRL, RX enable
        // RXCNT   =    7 << 16,   // RXCTRL, TXinterrupt threshold (TXWM = (len(FIFO) > RXCNT))
        TXWM       =    1 <<  0,   // IE/IP, TX water mark
        // RXWM    =    1 <<  1    // IE/IP, RX water mark
    };

    static constexpr unsigned int RECEIVE_IP_BIT = 4;
    static constexpr unsigned int TRANSMIT_IP_BIT = 0;

public:

    SPI(unsigned int clock, unsigned int protocol, unsigned int mode, unsigned int data_bits) {
        config(clock, protocol, mode, data_bits);
    }

    public:
    void config(unsigned int clock, unsigned int protocol, unsigned int mode, unsigned int data_bits){

        // Set the SPI registers
        // Configure the SPI controller registers
        reg(SCKDIV) = ((Traits<SPI>::CLOCK / (2 * clock)) - 1) & 0xFFF; //CLOCK EH O DA MAQUINA E O clock EH O SCLOCK
        reg(SCKMODE) &= ~0x3;                                           // Clear only the first 2 bits (0x3 = 0b11) //set phase and polarity to 0, their default value

        // Clear the FIFOs and enable the controller
        reg(CSID) = 0x0; // Set the chip select ID
        reg(CSDEF) = 0x0; // Set the chip select default
        reg(CSMODE) = mode << 0; // Set the chip select mode

        reg(DELAY0) = (reg(DELAY0) & ~(0xFF | (0xFF << 16))) | (0x1 | (0x1 << 16));
        // Clear bits 0 to 7 and bits 16 to 23
        reg(DELAY1) &= ~(0xFF | (0xFF << 16));

        // Set bits 0 to 7 to 0x1 and keep bits 16 to 23 as 0x0
        reg(DELAY1) |= 0x1;
        

        // Clear bits 0 to 2 and bits 16 to 19
        
        reg(FMT) = (protocol << 0) | (0 << 2) | (1 << 3) | (data_bits << 16); 
        
        // Clear bits 0 to 7
        reg(TXDATA) &= ~0xFF;
        // Clear bits 0 to 2
        reg(TXMARK) = 1 << 0;
        reg(RXMARK) &= ~(0x7);
        
        reg(FCTRL) |= 0x1;  // Enable the controller

    }
    int get_ie() {
        return reg(IE);
    }

    int get_ip() {
        return reg(IP);
    }

    int apply_frame_size(char data) {
        unsigned int all_ones = ~0;
        unsigned int frame_size = reg(FMT) & FMT_LEN >> 16;
        unsigned int frame_size_mask = ~(all_ones << frame_size);
        return data & frame_size_mask;
    }

   
    int get(){
        while(!ready_to_get());
        return reg(RXDATA) & 0xf;
    }


    void put(int data){
        while(!ready_to_put()); // Wait until the FIFO has room
        reg(TXDATA) = apply_frame_size(data);
    }

    int read(char * data, unsigned int max_size) {
        for(unsigned int i = 0; i < max_size; i++)
            data[i] = get();
        return 0;
    }

    // Sends an array of characters, using put
    int write(const char * data, unsigned int size) {
        for(unsigned int i = 0; i < size; i++)
            put(data[i]);
        return 0;
    }

    void flush() { while(!(reg(IP) & TXWM)); }

    bool ready_to_get() { return rxd_ok(); }
    bool ready_to_put() { return txd_ok(); }
    bool rxd_ok() { return !(reg(RXDATA) & EMPTY); }
    bool txd_ok() { return !(reg(TXDATA) & TXDATA_FULL); }

    void int_enable(bool receive = true, bool transmit = true, bool time_out = true, bool overrun = true) {
         reg(IE) = (receive << 1) | transmit;
    }
    void int_disable(bool receive = true, bool transmit = true, bool time_out = true, bool overrun = true) {
         reg(IE) = reg(IE) & ~((receive << 1) | transmit);
    }

    private:
    static volatile Reg32& reg(unsigned int offset) {
        return *reinterpret_cast<volatile Reg32*>(Memory_Map::SPI0_BASE + offset);
    }
};

__END_SYS

#endif


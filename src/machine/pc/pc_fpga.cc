// EPOS PC FPGA Mediator Implementation

#include <machine/machine.h>
#include <machine/ic.h>
#include <machine/fpga.h>

__BEGIN_SYS

// Class attributes
CPU::Log_Addr FPGA::Engine::_base;
MMU::DMA_Buffer * FPGA::Engine::_dma_buf;

// Class methods
void FPGA::int_handler(IC::Interrupt_Id i)
{

}

__END_SYS

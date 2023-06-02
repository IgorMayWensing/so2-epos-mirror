// EPOS RISC-V 64 System Call Entry Implementation

#include <architecture/rv64/rv64_cpu.h>

extern "C" { void _exec(void *); }

__BEGIN_SYS

#ifdef __kernel__

void CPU::syscalled(unsigned int int_id){}

#endif

#ifndef __library__

void CPU::Context::first_dispatch(){}

#endif


__END_SYS

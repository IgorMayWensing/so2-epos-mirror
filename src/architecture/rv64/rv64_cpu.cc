// EPOS RISC-V 64 CPU Mediator Implementation

#include <architecture/rv64/rv64_cpu.h>
#include <system.h>

__BEGIN_SYS

unsigned int CPU::_cpu_clock;
unsigned int CPU::_bus_clock;

void CPU::Context::save() volatile
{
    ASM("       csrr     x3,  sscratch          \n"     // sscratch = usp (sscratch holds ksp in user-land and usp in kernel; usp = 0 for kernel threads)
        "       sd       x3,    0(a0)           \n"     // push usp
        "       sd       x1,    8(a0)           \n");   // push lr as pc
if(multitask)
    ASM("       csrr     x3,  sstatus           \n");
else {
    ASM("       csrr     x3,  mstatus           \n");
}
    ASM("       sd       x3,   16(a0)           \n"     // push st
        "       sd       x1,   24(a0)           \n"     // push ra
        "       sd       x5,   32(a0)           \n"     // push x5-x31
        "       sd       x6,   40(a0)           \n"
        "       sd       x7,   48(a0)           \n"
        "       sd       x8,   56(a0)           \n"
        "       sd       x9,   64(a0)           \n"
        "       sd      x10,   72(a0)           \n"
        "       sd      x11,   80(a0)           \n"
        "       sd      x12,   88(a0)           \n"
        "       sd      x13,   96(a0)           \n"
        "       sd      x14,  104(a0)           \n"
        "       sd      x15,  112(a0)           \n"
        "       sd      x16,  120(a0)           \n"
        "       sd      x17,  128(a0)           \n"
        "       sd      x18,  136(a0)           \n"
        "       sd      x19,  144(a0)           \n"
        "       sd      x20,  152(a0)           \n"
        "       sd      x21,  160(a0)           \n"
        "       sd      x22,  168(a0)           \n"
        "       sd      x23,  176(a0)           \n"
        "       sd      x24,  184(a0)           \n"
        "       sd      x25,  192(a0)           \n"
        "       sd      x26,  200(a0)           \n"
        "       sd      x27,  208(a0)           \n"
        "       sd      x28,  216(a0)           \n"
        "       sd      x29,  224(a0)           \n"
        "       sd      x30,  232(a0)           \n"
        "       sd      x31,  240(a0)           \n"
        "       ret                             \n");
}

// Context load does not verify if interrupts were previously enabled by the Context's constructor
// We are setting mstatus to MPP | MPIE, therefore, interrupts will be enabled only after mret
void CPU::Context::load() const volatile
{
    sp(Log_Addr(this));
    pop();
    iret();
}

void CPU::switch_context(Context ** o, Context * n)     // "o" is in a0 and "n" is in a1
{   
    // Push the context into the stack and update "o"
    Context::push();
    ASM("sd sp, 0(a0)");   // update Context * volatile * o, which is in a0

    // Set the stack pointer to "n" and pop the context from the stack
    ASM("mv sp, a1");   // "n" is in a1
    Context::pop();
    iret();
}

__END_SYS


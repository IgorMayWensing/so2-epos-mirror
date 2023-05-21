#include <system.h>
// EPOS Kernel Binding

__BEGIN_SYS
__END_SYS

__USING_SYS;
#ifdef __ia32__
extern "C" { void _exec(void *) __attribute__ ((thiscall)); }
#endif

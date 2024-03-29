// EPOS ARMv7 Program Starter

        .file "armv7_crt0.S"

        .align  2
        .global _start
        .type   _start, function
_start:

        // Temporary stack(s) for INIT were created and configured by SETUP
#ifdef __cortex_m__
        // Clear the BSS
        bl      _clear_bss
#else
        // BSS was cleared by SETUP
#endif
        push    {r0, r1}     // save argc and argv
        bl      _init

        .align  2
        .globl  __epos_app_entry
        .type   __epos_app_entry, function
__epos_app_entry:
        // Main's stack was allocated by Thread::init()
        pop    {r0, r1}      // recover argc and argv
        bl      main
        push    {r0}         // save main's return value to be used by exit()
        bl      _fini
        bl      _exit

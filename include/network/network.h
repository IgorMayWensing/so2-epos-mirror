// EPOS Network Common Package

#ifndef __Network_h
#define __Network_h

#include <system/config.h>

extern "C" { void __pre_main(); }

__BEGIN_SYS

class Network
{
     friend void ::__pre_main();

private:
    static void init();
};

__END_SYS

#endif

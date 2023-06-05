// EPOS Light Protocol Initialization

#include <system/config.h>

#ifdef __NIC_H

#include <network/elp.h>

__BEGIN_SYS

void ELP::init()
{
    db<Init, ELP>(TRC) << "ELP::init()" << endl;
    init_helper<0>();
}

__END_SYS

#endif

// EPOS Network Component Initialization

#include <network.h>

#ifdef __NIC_H

__BEGIN_SYS

void Network::init()
{
    db<Init, Network>(TRC) << "Network::init()" << endl;

    if(Traits<ELP>::enabled)
        ELP::init();

#ifdef __ipv4__

    if(Traits<IP>::enabled)
        IP::init();

#endif

#ifdef __tstp__

    if(Traits<TSTP>::enabled)
        TSTP::init();

#endif

}

__END_SYS

#endif

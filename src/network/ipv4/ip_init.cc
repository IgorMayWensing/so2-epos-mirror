// EPOS IP Protocol Initialization

#include <network/network.h>
#include <network/ipv4/ip.h>
#include <network/ipv4/icmp.h>
#include <network/ipv4/udp.h>
#include <network/ipv4/tcp.h>

#ifdef __ipv4__

__BEGIN_SYS

IP::IP(NIC<Ethernet> * nic, unsigned int config, const Address & a, const Address & m, const Address & g)
: _nic(nic), _arp(_nic, this), _address(a), _netmask(m), _broadcast((_address & _netmask) | ~_netmask), _gateway(g)
{
    db<IP>(TRC) << "IP::IP(nic=" << _nic << ",ip=" << a << ",nm=" << m << ",gw=" << g << ",cfg=" << config << ") => " << this << endl;

    _nic->attach(this, NIC<Ethernet>::PROTO_IP);

    switch(config) {
    case Traits<IP>::STATIC:                    break;
    case Traits<IP>::MAC:    config_by_mac();   break;
    case Traits<IP>::INFO:   config_by_info();  break;
//    case Traits<IP>::RARP:   config_by_rarp();  break;
    case Traits<IP>::DHCP:   config_by_dhcp();  break;
    default:
        db<IP>(ERR) << "IP::IP:config " << config << " not supported" << endl;
    }

    _router.insert(_nic, this, &_arp, _address & _netmask, _address, _netmask);

    if(_gateway) {
        _router.insert(_nic, this, &_arp, Address::NULL, _gateway, Address::NULL); // Default route must be the last one in table
        _arp.resolve(_gateway);
    }
}

void IP::init()
{
    db<Init, IP>(TRC) << "IP::init()" << endl;
    init_helper<0>();

    if(Traits<_SYS::ICMP>::enabled)
        new (SYSTEM) _SYS::ICMP;
    if(Traits<_SYS::UDP>::enabled)
        new (SYSTEM) _SYS::UDP;
    if(Traits<_SYS::TCP>::enabled)
        new (SYSTEM) _SYS::TCP;
}

__END_SYS

#endif

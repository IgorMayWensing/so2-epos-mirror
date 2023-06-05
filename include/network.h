// EPOS Network Mediator

#ifndef __network_h
#define __network_h

#include <machine/nic.h>
#include <network/network.h>

#ifdef __NIC_H
#include <network/elp.h>
#endif

#ifdef __ipv4__
#include <network/ipv4/ip.h>
#include <network/ipv4/tcp.h>
#include <network/ipv4/udp.h>
#endif

#ifdef __tstp__
#include <network/tstp/tstp.h>
#endif

#endif

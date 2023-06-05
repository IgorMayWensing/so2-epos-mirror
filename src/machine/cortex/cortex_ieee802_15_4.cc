// EPOS Cortex  IEEE 802.15.4 NIC Mediator Implementation

#include <system.h>

#ifdef __ieee802_15_4__

#include <machine/cortex/cortex_ieee802_15_4.h>

__BEGIN_SYS

IEEE802_15_4_NIC::Device IEEE802_15_4_NIC::_devices[UNITS];

IEEE802_15_4_NIC::~IEEE802_15_4_NIC()
{
    db<IEEE802_15_4_NIC>(TRC) << "~IEEE802_15_4_NIC(unit=" << _configuration.unit << ")" << endl;
}

int IEEE802_15_4_NIC::send(const Address & dst, const IEEE802_15_4::Type & type, const void * data, unsigned int size)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::send(s=" << address() << ",d=" << dst << ",p=" << hex << type << dec << ",d=" << data << ",s=" << size << ")" << endl;

    Buffer * b = alloc(dst, type, 0, 0, size);
    memcpy(b->frame()->data<void>(), data, size);
    return send(b);
}

int IEEE802_15_4_NIC::receive(Address * src, IEEE802_15_4::Type * type, void * data, unsigned int size)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::receive(s=" << *src << ",p=" << hex << *type << dec << ",d=" << data << ",s=" << size << ") => " << endl;

    Buffer * buf;
    for(buf = 0; !buf; ++_rx_cur_consume %= RX_BUFS) { // _xx_cur_xxx are simple accelerators to avoid scanning the ring buffer from the beginning.
                                                       // Losing a write in a race condition is assumed to be harmless. The FINC + CAS alternative seems too expensive.
        unsigned int idx = _rx_cur_consume;
        if(_rx_bufs[idx]->lock()) {
            if(_rx_bufs[idx]->size() > 0)
                buf = _rx_bufs[idx];
            else
                _rx_bufs[idx]->unlock();
        }
    }

    Address dst;

    unsigned int ret = 0;

#ifdef __tstp__
    if(Traits<IEEE802_15_4_NIC>::tstp_mac)
        ret= MAC::unmarshal(buf, data, size);
    else
#endif
        ret= MAC::unmarshal(buf, src, &dst, type, data, size);

    free(buf);

    return ret;
}

IEEE802_15_4::Buffer * IEEE802_15_4_NIC::alloc(const Address & dst, const IEEE802_15_4::Type & type, unsigned int once, unsigned int always, unsigned int payload)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::alloc(s=" << address() << ",d=" << dst << ",p=" << hex << type << dec << ",on=" << once << ",al=" << always << ",ld=" << payload << ")" << endl;

    // Initialize the buffer
    Buffer * buf = new (SYSTEM) Buffer(this, 0);
    buf->size(once + always + payload + sizeof(IEEE802_15_4::Header));
    if (Traits<IEEE802_15_4_NIC>::tstp_mac) {
        MAC::marshal(buf);
    } else {
        MAC::marshal(buf, address(), dst, type);
    }
    return buf;
}

int IEEE802_15_4_NIC::send(Buffer * buf)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::send(buf=" << buf << ",frame=" << buf->frame() << " => " << *(buf->frame()) << endl;

    unsigned int size = MAC::send(buf);

    if(size) {
        _statistics.tx_packets++;
        _statistics.tx_bytes += size;
    } else
        db<IEEE802_15_4_NIC>(WRN) << "IEEE802_15_4_NIC::send(buf=" << buf << ")" << " => failed!" << endl;

    return size;
}

void IEEE802_15_4_NIC::free(Buffer * buf)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::free(buf=" << buf << ")" << endl;

    _statistics.rx_packets++;
    _statistics.rx_bytes += buf->size();

    buf->size(0);
    buf->unlock();
}

bool IEEE802_15_4_NIC::reconfigure(const Configuration * c)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::reconfigure(c=" << c << ")" << endl;

    bool ret = false;
    if(!c) {
        CPU::int_disable();
        ret = MAC::reset();
        new (&_statistics) Statistics; // reset statistics
        CPU::int_enable();
    } else {
        db<IEEE802_15_4_NIC>(INF) << "IEEE802_15_4_NIC::reconfigure: configuration = " << *c << ")" << endl;

        if(c->selector & Configuration::ADDRESS) {
            Engine::address(c->address);
            if(Engine::address() == c->address) {
                _configuration.address = c->address;
                ret = true;
            }
        }

        if(c->selector & Configuration::CHANNEL) {
            Engine::channel(c->channel);
            if(Engine::channel() == c->channel) {
                _configuration.channel = c->channel;
                ret = true;
            }
        }

        if(c->selector & Configuration::POWER) {
            Engine::tx_power(c->power);
            if(Engine::tx_power() == c->power) {
                _configuration.power = c->power;
                ret = true;
            }
        }

        if(c->selector & Configuration::PERIOD) {
            // TODO: implement period reconfiguration
        }

        if(c->selector & Configuration::TIMER) {
            if(c->parameter) {
                Engine::timer_adjust(c->parameter);
                ret = true;
            }
            if(c->timer_frequency) {
                db<PCNet32>(WRN) << "IEEE802_15_4_NIC::reconfigure: timer frequency cannot be changed!" << endl;
                ret = false;
            }
        }
    }

    return ret;
}

void IEEE802_15_4_NIC::handle_int()
{
    Engine::Timer::Time_Stamp sfdts = Engine::sfdts();

    bool handle_frame = Engine::handle_int();

    if(handle_frame) {
        Buffer * buf = 0;
        unsigned int idx = _rx_cur_produce;
        for(unsigned int count = RX_BUFS; count; count--, ++idx %= RX_BUFS) {
            if(_rx_bufs[idx]->lock()) {
                buf = _rx_bufs[idx];
                break;
            }
        }
        _rx_cur_produce = (idx + 1) % RX_BUFS;

        if(buf) {
            buf->size(Engine::copy_from_nic(buf->frame()));
            if(buf->collect_sfdts)
                buf->sfdts = sfdts;
            if(buf->collect_rssi)
                buf->rssi = reinterpret_cast<char *>(buf->frame())[buf->size() - 2]; // RSSI appended to the frame
            if(MAC::pre_notify(buf)) {
           	db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int() calling notify..."<< endl;
                bool notified = notify(4, buf); // buf->frame()->header()->type(), buf);
            	db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int() calling post_notify..."<< endl;
                if(!MAC::post_notify(buf) && !notified) {
                	db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int() calling unlock..."<< endl;
                    buf->unlock(); // No one was waiting for this frame, so make it available for receive()
                }
            } else {
                db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int: frame dropped by MAC"  << endl;
                buf->size(0);
                buf->unlock();
            }
        } else {
            db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int: dropped" << endl;
            Engine::drop();
        }
    }
}

void IEEE802_15_4_NIC::int_handler(IC::Interrupt_Id interrupt)
{
    IEEE802_15_4_NIC * dev = get_by_interrupt(interrupt);

    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::int_handler(int=" << interrupt << ",dev=" << dev << ")" << endl;

    if(!dev)
        db<IEEE802_15_4_NIC>(WRN) << "IEEE802_15_4_NIC::int_handler: handler not assigned!" << endl;
    else {
        IC::disable(IC::INT_NIC0_TIMER); // Make sure NIC and MAC timer don't preempt one another
        dev->handle_int();
        IC::enable(IC::INT_NIC0_TIMER);
    }
}

// TSTP binding
#ifdef __tstp__

//template<typename Engine>
//void TSTP::MAC<Engine, true>::free(Buffer * b) { IEEE802_15_4_NIC::get()->free(b); }
//
//template<typename Engine>
//void TSTP::MAC<Engine, false>::free(Buffer * b) { IEEE802_15_4_NIC::get()->free(b); }

#endif

__END_SYS

#endif

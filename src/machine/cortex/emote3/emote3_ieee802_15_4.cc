// EPOS EPOSMoteIII (ARM Cortex-M3) IEEE 802.15.4 NIC Mediator Implementation

#include <system/config.h>

#ifdef __ieee802_15_4__

#include <machine/cortex/cortex_ieee802_15_4.h>

__BEGIN_SYS

IEEE802_15_4_Engine::Timer * IEEE802_15_4_Engine::_eng_timer;

bool CC2538RF::handle_int()
{
    bool handle_frame = false;

    Reg32 irqrf0 = sfr(RFIRQF0);
    Reg32 irqrf1 = sfr(RFIRQF1);
    Reg32 errf = sfr(RFERRF);
    sfr(RFIRQF0) = irqrf0 & INT_RXPKTDONE; //INT_RXPKTDONE is polled by rx_done()
    sfr(RFIRQF1) = irqrf1 & INT_TXDONE; //INT_TXDONE is polled by tx_done()
    sfr(RFERRF) = errf & (INT_TXUNDERF | INT_TXOVERF);
    if(Traits<IEEE802_15_4_NIC>::hysterically_debugged) {
        db<IEEE802_15_4_NIC>(TRC) << "CC2538RF::handle_int()" << endl;

        db<IEEE802_15_4_NIC>(INF) << "CC2538RF::handle_int:RFIRQF0=" << hex << irqrf0 << endl;
        db<IEEE802_15_4_NIC>(INF) << "CC2538RF::handle_int:RFIRQF1=" << hex << irqrf1 << endl;
        db<IEEE802_15_4_NIC>(INF) << "CC2538RF::handle_int:RFERRF=" << hex << errf << endl;
    }

    if(errf & (INT_RXUNDERF | INT_RXOVERF)) { // RX Error
        db<IEEE802_15_4_NIC>(INF) << "CC2538RF::handle_int:RFERRF=" << hex << errf << endl;
        drop();
    } else if(irqrf0 & INT_FIFOP) { // Frame received
        if(Traits<IEEE802_15_4_NIC>::hysterically_debugged)
            db<IEEE802_15_4_NIC>(TRC) << "CC2538RF::handle_int:receive()" << endl;
        if(filter())
            handle_frame = true;
        else
            db<IEEE802_15_4_NIC>(TRC) << "CC2538RF::handle_int: frame filtered out!" << endl;
    } else {
        db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::handle_int: NOT FIFOP" << endl;
    }

    return handle_frame;
}

void IEEE802_15_4_Engine::Timer::handle_int(IC::Interrupt_Id interrupt)
{
    CPU::int_disable();
    Interrupt_Mask interrupts = _interrupts;
    _interrupts = 0;

    if(interrupts & CC2538RF::INT_OVERFLOW_PER)
        _overflow_count++;

    if(_handler && (interrupts & CC2538RF::INT_COMPARE1) && (_handler_activation_time <= time_stamp())) {
        if(Traits<TSTP>::hysterically_debugged)
            db<IEEE802_15_4>(INF) << 't';
        CC2538RF::Timer * _timer = new (reinterpret_cast<void *>(Memory_Map::RF_BASE + CC2538RF::MACTIMER)) CC2538RF::Timer;
        Handler h = _handler;
        _handler = 0;
        _timer->int_disable(~CC2538RF::INT_OVERFLOW_PER); // disable all but tick counting until a next handler is configured
        // db<IEEE802_15_4_NIC>(TRC) << "\nIEEE802_15_4_Engine::Timer::handle_int calling method!!!" << endl;
        h(interrupt);
    }
    IC::enable(IC::INT_NIC0_RX); // Make sure radio and MAC timer don't preempt one another
    CPU::int_enable();
}

void IEEE802_15_4_Engine::Timer::timer_int_handler(IC::Interrupt_Id interrupt)
{
    _eng_timer->handle_int(interrupt);
}

void IEEE802_15_4_Engine::Timer::eoi(const IC::Interrupt_Id & interrupt)
{
    CC2538RF::Timer * _timer = new (reinterpret_cast<void *>(Memory_Map::RF_BASE + CC2538RF::MACTIMER)) CC2538RF::Timer;
    _eng_timer->_interrupts |= _timer->eoi();
    IC::disable(IC::INT_NIC0_RX); // Make sure radio and MAC timer don't preempt one another
}

IEEE802_15_4_Engine * IEEE802_15_4_Engine::_instance = 0;

IEEE802_15_4_Engine * IEEE802_15_4_Engine::instance() {
    if (_instance == 0)
        new IEEE802_15_4_Engine(); // _instance is updated in constructor
    return _instance;
}

__END_SYS

#endif

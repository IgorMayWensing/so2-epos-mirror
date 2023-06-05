// EPOS Cortex  IEEE 802.15.4 NIC Mediator Initialization

#include <system.h>

#ifdef __ieee802_15_4__

#include <machine/cortex/cortex_ieee802_15_4.h>

__BEGIN_SYS

IEEE802_15_4_NIC::IEEE802_15_4_NIC(unsigned int unit): _rx_cur_consume(0), _rx_cur_produce(0)
{
    db<IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC(unit=" << unit << ") => " << this << endl;

    _configuration.unit = unit;

    // Initialize RX buffer pool
    for(unsigned int i = 0; i < RX_BUFS; i++)
        _rx_bufs[i] = new (SYSTEM) Buffer(this, 0);
    // Put the radio in FULL power mode (at this point, it was OFF)
    power(FULL);
    // Set Address
    const UUID & id = Machine::uuid();
    _configuration.address[0] = id[4] ^ id[5];
    _configuration.address[1] = id[6] ^ id[7];
    address(_configuration.address);

    _configuration.channel = Traits<IEEE802_15_4_NIC>::DEFAULT_CHANNEL;
    _configuration.timer_accuracy = timer_accuracy();
    _configuration.timer_frequency = timer_frequency();

    // Reset
    reconfigure(&_configuration);
    Engine::init();
    IEEE802_15_4_NIC::MAC::constructor_epilogue(); // Device is configured, let the MAC use it
}


void IEEE802_15_4_NIC::init(unsigned int unit)
{
    db<Init, IEEE802_15_4_NIC>(TRC) << "IEEE802_15_4_NIC::init(unit=" << unit << ")" << endl;

    // Initialize the device
    IEEE802_15_4_NIC * dev = new (SYSTEM) IEEE802_15_4_NIC(unit);

    // Register the device
    _devices[unit].device = dev;
    _devices[unit].interrupt = IC::INT_NIC0_RX;
    _devices[unit].error_interrupt = IC::INT_NIC0_ERR;

    // Install interrupt handler
    IC::int_vector(_devices[unit].interrupt, &int_handler);
    IC::int_vector(_devices[unit].error_interrupt, &int_handler);

    // Enable interrupts for device
    IC::enable(IC::INT_NIC0_RX);
    IC::enable(IC::INT_NIC0_ERR);
}

__END_SYS

#endif

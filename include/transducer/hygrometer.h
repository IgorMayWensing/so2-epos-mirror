// EPOS Relative Humidity Smart Transducer Declarations

#ifndef __hygrometer_h
#define __hygrometer_h

#include <transducer.h>

#ifdef __mmod_emote3__

#include <machine/engine/si7020.h>
#include <machine/engine/cm1101.h>
#include <machine/i2c.h>

__BEGIN_SYS

// Silicon Labs SI7020 Relative Humidity sensor
class Hygrometer: public Transducer<SmartData::Unit::I32 | SmartData::Unit::Percent>, private I2C, private SI7020
{
public:
    static const unsigned int DEVS = 1;

    static const int ERROR = 4; // ±0.4 °C (max), –10 to 85 °C
    static const bool active = false;

public:
    Hygrometer(unsigned int dev = 0): I2C(Traits<I2C>::SI7020_UNIT), SI7020(this) { assert(dev < DEVS); }

    Value sense() { return humidity(); }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        sd->_value = humidity();
    }
};

class Alternate_Hygrometer: public Transducer<SmartData::Unit::I32 | SmartData::Unit::Percent>, private UART, private CM1101
{
public:
    static const unsigned int DEVS = 1;

    static const int ERROR = 4; // ±0.4 °C (max), –10 to 85 °C
    static const bool active = false;

public:
    Alternate_Hygrometer(unsigned int dev = 0): UART(Traits<UART>::CM1101_UNIT), CM1101(this) { assert(dev < DEVS); }

    Value sense() { return humidity(); }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        sd->_value = humidity();
    }
};

__END_SYS

#endif

#endif

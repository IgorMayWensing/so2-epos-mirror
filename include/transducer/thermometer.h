// EPOS Temperature Smart Transducer Declarations

#ifndef __thermometer_h
#define __thermometer_h

#include <transducer.h>

#ifdef __mmod_emote3__

#include <utility/convert.h>
#include <machine/engine/si7020.h>
#include <machine/engine/cm1101.h>
#include <machine/i2c.h>

__BEGIN_SYS

// Silicon Labs SI7020 Temperature sensor
class Thermometer: public Transducer<SmartData::Unit::I32 | SmartData::Unit::Temperature>, private I2C, private SI7020
{
    friend Hygrometer;

public:
    static const unsigned int DEVS = 1;

    static const int ERROR = 4; // ±0.4 °C (max), –10 to 85 °C
    static const bool active = false;

public:
    Thermometer(unsigned int dev = 0): I2C(Traits<I2C>::SI7020_UNIT, I2C::MASTER), SI7020(this) { assert(dev < DEVS); }

    Value sense() { return Convert::c2k(temperature()); }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        sd->_value = Convert::c2k(temperature());
    }
};

class Alternate_Thermometer: public Transducer<SmartData::Unit::I32 | SmartData::Unit::Percent>, private UART, private CM1101
{
    friend Alternate_Hygrometer;

public:
    static const unsigned int DEVS = 1;

    static const int ERROR = 4; // ±0.4 °C (max), –10 to 85 °C
    static const bool active = false;

public:
    Alternate_Thermometer(unsigned int dev = 0): UART(Traits<UART>::CM1101_UNIT), CM1101(this) { assert(dev < DEVS); }

    Value sense() { return Convert::c2k(temperature()); }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        sd->_value = Convert::c2k(temperature());
    }
};

__END_SYS

#endif

#endif

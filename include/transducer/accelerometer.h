// EPOS Acceleration Smart Transducer Declarations

#ifndef __accelerometer_h
#define __accelerometer_h

#include <transducer.h>

#ifdef __mmod_emote3__

#include <utility/convert.h>
#include <machine/engine/lsm330.h>
#include <machine/i2c.h>

__BEGIN_SYS

class Accelerometer: public Transducer<SmartData::Unit::F32 | SmartData::Unit::Acceleration>, private I2C, private LSM330
{
    friend Gyroscope;

public:
    static const unsigned int DEVS = 3;

    static const Uncertainty UNCERTAINTY = UNKNOWN;
    static const bool active = false;

public:
    Accelerometer(unsigned int dev = 0): I2C(Traits<I2C>::LSM330_UNIT, I2C::MASTER), LSM330(this), _dev(dev) {
        assert(dev < DEVS);

        accelerometer_config();
    }

    Value sense() {
        Value tmp;
        switch(_dev) {
        case 0: tmp = Convert::g2ms2(accelerometer_x()); break;
        case 1: tmp = Convert::g2ms2(accelerometer_y()); break;
        default: tmp = Convert::g2ms2(accelerometer_z());
        }
        return tmp;
    }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        switch(dev) {
        case 0: sd->_value = Convert::g2ms2(accelerometer_x()); break;
        case 1: sd->_value = Convert::g2ms2(accelerometer_y()); break;
        default: sd->_value = Convert::g2ms2(accelerometer_z());
        }
    }

private:
    unsigned int _dev;
};

__END_SYS

#endif

#endif

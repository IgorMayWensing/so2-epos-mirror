// EPOS Angular Velocity Smart Transducer Declarations

#ifndef __gyroscope_h
#define __gyroscope_h

#include <transducer.h>

#ifdef __mmod_emote3__

#include <utility/convert.h>
#include <machine/engine/lsm330.h>
#include <machine/i2c.h>

__BEGIN_SYS

class Gyroscope: public Transducer<SmartData::Unit::F32 | SmartData::Unit::Angular_Velocity>, private I2C, private LSM330
{
public:
    static const unsigned int DEVS = 3;

    static const int ERROR = 61000;
    static const bool active = false;

public:
    Gyroscope(unsigned int dev = 0): I2C(Traits<I2C>::LSM330_UNIT, I2C::MASTER), LSM330(this), _dev(dev) {
        // _i2c is being initialized after its being passed to LSM330, but the constructor is empty. Otherwise, use the comma operator
        assert(dev < DEVS);

        gyroscope_config();
    }

    Value sense() {
        Value tmp;
        switch(_dev) {
        case 0: tmp = Convert::deg2rad(gyroscope_x()); break;
        case 1: tmp = Convert::deg2rad(gyroscope_y()); break;
        default: tmp = Convert::deg2rad(gyroscope_z());
        }
        return tmp;
    }

    template<typename SD>
    void sense(unsigned int dev, SD * sd) {
        switch(dev) {
        case 0: sd->_value = Convert::deg2rad(gyroscope_x()); break;
        case 1: sd->_value = Convert::deg2rad(gyroscope_y()); break;
        default: sd->_value = Convert::deg2rad(gyroscope_z());
        }
    }

private:
    unsigned int _dev;
};

__END_SYS

#endif

#endif

#include <machine.h>
#include <transducer.h>

using namespace EPOS;

OStream cout;

int main()
{
    cout << "Transducer test" << endl;

    cout << "This is an accelerometer test." << endl;

    cout << "  The red led will blink throughout the test." << endl;
    GPIO led(GPIO::C, 3, GPIO::OUT);
    led.set();

    const unsigned int FREQUENCY = 120; // Hz
    const unsigned int PERIOD = ((1/FREQUENCY)*1000000);

    Accelerometer ax(0), ay(1), az(2);
    Gyroscope gx(0), gy(1), gz(2);

    int count = 0;
    bool led_on = false;
    while(true) {
        if(count++ % 100) {
            led.set(led_on);
            led_on = !led_on;
        }
        cout << "  Acceleration     -> (X=" << ax.sense() << ", Y=" << ay.sense() << ",Z=" << az.sense() <<") g" << endl;
        cout << "  Angular Velocity -> (X=" << gx.sense() << ", Y=" << gy.sense() << ",Z=" << gz.sense() <<") g" << endl;
        Alarm::delay(PERIOD);
    }

    return 0;
}

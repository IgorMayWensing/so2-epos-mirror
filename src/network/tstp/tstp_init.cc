// EPOS Trustful SpaceTime Protocol Initialization

#include <system/config.h>

#ifdef __tstp__

#include <machine/nic.h>
#include <network/tstp/tstp.h>
#include <system.h>
#include <time.h>

__BEGIN_SYS

TSTP::TSTP(NIC<NIC_Family> * nic)
{
    db<Init, TSTP>(TRC) << "TSTP(nic=" << nic << ")" << endl;

    _nic = nic;
    _nic->attach(this, PROTO_TSTP);

    // The order parts are created defines the order they get notified when packets arrive:
    // mac->security(decrypt)->locator->timekeeper->router->manager->security(encrypt)->mac
    _security = new (SYSTEM) Security;
    _locator = new (SYSTEM) Locator;
    _timekeeper = new (SYSTEM) Timekeeper; // here() reports (0,0,0) if _locator wasn't created first!
    _router = new (SYSTEM) Router;
    _manager = new (SYSTEM) Manager;
}

TSTP::Security::Security()
{
    db<TSTP>(TRC) << "TSTP::Security()" << endl;

    new (&_id) Node_Id(Machine::uuid(), sizeof(UUID));

    db<TSTP>(INF) << "TSTP::Security:uuid=" << _id << endl;

    _aes.encrypt(_id, _id, _auth);

    attach(this);

    // initialization continues through update as relevant packets are received

    /*
    // TODO: what is this?
    if((TSTP::here() != TSTP::sink()) && (!Traits<Radio>::promiscuous)) {
        Peer * peer = new (SYSTEM) Peer(_id, Region(TSTP::sink(), 0, 0, -1));
        _pending_peers.insert(peer->link());

        // Wait for key establishment
        while(_trusted_peers.size() == 0)
            Thread::self()->yield();
    }
    */
}

TSTP::Locator::Locator()
{
    db<TSTP>(TRC) << "TSTP::Locator()" << endl;

    System_Info::Boot_Map * bm = &System::info()->bm;
    if(bm->space_x != Space::UNKNOWN) {
        _engine.here(Space(bm->space_x, bm->space_y, bm->space_z));
        _engine.confidence(100);
    } else {
        _engine.here(Space(Space::UNKNOWN, Space::UNKNOWN, Space::UNKNOWN));
        _engine.confidence(0);
    }

    attach(this);

    db<TSTP>(INF) << "TSTP::Locator:here=" << here();
    if(here() == sink())
        db<TSTP>(INF) << "[sink]" << endl;
    else
        db<TSTP>(INF) << "[node]" << endl;

    // Wait for spatial localization
    while(confidence() < 80)
        Thread::self()->yield();

    // _absolute_location is initialized later through an Epoch message
}

TSTP::Timekeeper::Timekeeper()
{
    db<TSTP>(TRC) << "TSTP::Timekeeper()" << endl;
    db<TSTP>(INF) << "TSTP::Timekeeper:timer accuracy = " << timer_accuracy() << " ppb" << endl;
    db<TSTP>(INF) << "TSTP::Timekeeper:timer frequency = " << timer_frequency() << " Hz" << endl;
    db<TSTP>(INF) << "TSTP::Timekeeper:maximum drift = " << MAX_DRIFT << " us" << endl;
    db<TSTP>(INF) << "TSTP::Timekeeper:sync period = " << sync_period() << " us" << endl;

    attach(this);

    if(here() == sink())
        _next_sync = INFINITE; // just so that the sink will always have synchronized() returning true
    else {
        _next_sync = 0;
        keep_alive();
        Microsecond period = static_cast<Microsecond>(sync_period());
        _life_keeper_handler = new (SYSTEM) Function_Handler(&keep_alive);
        _life_keeper = new (SYSTEM) Alarm(period, _life_keeper_handler, INFINITE);
        while(!synchronized())
            Thread::self()->yield();
    }
}

TSTP::Router::Router()
{
    db<TSTP>(TRC) << "TSTP::Router()" << endl;

    attach(this);
}

TSTP::Manager::Manager()
{
    db<TSTP>(TRC) << "TSTP::Manager()" << endl;

    attach(this);
}

void TSTP::init()
{
    db<Init, TSTP>(TRC) << "TSTP::init()" << endl;

    NIC<NIC_Family> * nic = Traits<NIC_Family>::DEVICES::Get<Traits<TSTP>::NICS[0]>::Result::get(Traits<TSTP>::NICS[0]);

    new (SYSTEM) TSTP(nic);
}

//template <typename Engine>
//TSTP::MAC<Engine, true> TSTP::MAC<Engine, true>::_instance;

//template <typename Engine>
//TSTP::MAC<Engine, false> TSTP::MAC<Engine, false>::_instance;

//template <typename Engine>
//TSTP::MAC<Engine, true> TSTP::MAC<Engine, true>::instance() {
//    return _instance;
//}

//template <typename Engine>
//TSTP::MAC<Engine, false> TSTP::MAC<Engine, false>::instance() {
//    return _instance;
//}

__END_SYS

#endif

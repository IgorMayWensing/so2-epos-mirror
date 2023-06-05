// EPOS Trustful SpaceTime Protocol MAC Declarations

// Differently from other components of TSTP, which can also run on Ethernet, the MAC is only defined for IEEE 802.15.4

#include <system/config.h>

#if defined(__temporarely_disabled__) && !defined (__tstp_mac_h) && defined (__ieee802_15_4__) && defined (__tstp__)
#define __tstp_mac_h

#include <machine/ic.h>
#include <machine/timer.h>
#include <machine/watchdog.h>
#define __tstp_common_only__
#include <network/tstp/tstp.h>
#undef __tstp_common_only__
#include <utility/random.h>
#include <utility/math.h>

__BEGIN_SYS

// TSTP::MAC with duty cycling and microframes
template<typename Engine>
class TSTP::MAC<Engine, true>: private SmartData, public Engine
{

friend IEEE802_15_4_NIC;

private:
    typedef CPU::Reg16 Frame_ID;
    typedef CPU::Reg32 Hint;
    typedef CPU::Reg16 MF_Count;
    typedef NIC_Common::CRC16 CRC;
    typedef IEEE802_15_4 Phy_Layer;
    static TSTP::MAC<Engine, true> _instance;

    enum State {
        UPDATE_TX_SCHEDULE = 0,
        SLEEP_S            = 1,
        RX_MF              = 2,
        SLEEP_DATA         = 3,
        RX_DATA            = 4,
        CONTEND            = 5,
        CCA                = 6,
        TX_MF              = 7,
        TX_DATA            = 8,
    };

    // MAC Preamble Microframe
    class Microframe
    {
        // Format
        // Bit 0            1      12   24     56    72
        //     +------------+-------+----+------+-----+
        //     | all listen | count | id | hint | crc |
        //     +------------+-------+----+------+-----+
        // Bits       1        11     12    32    16
    public:
        Microframe() {}

        Microframe(bool all_listen, const Frame_ID & id, const MF_Count & count, const Hint & hint = 0)
        : _al_count_id_hintl(htole32(all_listen | ((count & 0x07ff) << 1) | ((id & 0x0fff) << 12) | ((hint & 0x0ff) << 24))), _hinth_crcl(htole32((hint & 0xffffff00) >> 8)), _crch(0) {}

        MF_Count count() const { return (letoh32(_al_count_id_hintl) & 0x0ffe) >> 1; }

        MF_Count dec_count() {
            MF_Count c = count();
            count(c - 1);
            return c;
        }

        void count(const MF_Count & c) {
            _al_count_id_hintl = htole32((letoh32(_al_count_id_hintl) & ~0x0ffe) | (c << 1));
        }

        Frame_ID id() const { return ((letoh32(_al_count_id_hintl) & 0x00fff000) >> 12); }

        void id(const Frame_ID & id) {
            _al_count_id_hintl = htole32((letoh32(_al_count_id_hintl) & ~0x00fff000) | ((id & 0x00000fff) << 12));
        }

        void all_listen(bool all_listen) {
            if(all_listen)
                _al_count_id_hintl = htole32((letoh32(_al_count_id_hintl) | 0x01));
            else
                _al_count_id_hintl = htole32((letoh32(_al_count_id_hintl) & ~0x01));
        }

        bool all_listen() const { return letoh32(_al_count_id_hintl & 0x01); }

        Hint hint() const { return ((letoh32(_al_count_id_hintl) & 0xff000000) >> 24) + ((letoh32(_hinth_crcl) & 0x00ffffff) << 8); }
        void hint(const Hint & h) {
            _al_count_id_hintl = htole32((letoh32(_al_count_id_hintl) & ~0xff000000) | ((h & 0x0ff) << 24));
            _hinth_crcl = htole32((letoh32(_hinth_crcl) & ~0x00ffffff) | ((static_cast<unsigned int>(h & 0xffffff00)) >> 8));
        }

        friend Debug & operator<<(Debug & db, const Microframe & m) {
            db << "{al=" << m.all_listen() << ",c=" << m.count() << ",id=" << m.id() << ",h=" << m.hint() << "}";
            return db;
        }
        friend OStream & operator<<(OStream & db, const Microframe & m) {
            db << "{al=" << m.all_listen() << ",c=" << m.count() << ",id=" << m.id() << ",h=" << m.hint()<< "}";
            return db;
        }

    private:
        unsigned int _al_count_id_hintl; // All listen, Count, ID, Hint LSB
        unsigned int _hinth_crcl; // Hint MSBs, CRC LSB
        unsigned char _crch; // CRC MSB
    } __attribute__((packed));


public:
    typedef typename Engine::Timer Timer;
    typedef typename Engine::Timer::Time_Stamp Time_Stamp;
    typedef typename IEEE802_15_4::Buffer Buffer; // here Buffer MUST be the TSTP one.
    static unsigned int period() { return PERIOD; }

private:
    static const bool sniffer = Traits<Traits<IEEE802_15_4>::DEVICES::Get<0>::Result>::promiscuous;
    static const bool state_machine_debugged = false;
    static const bool random_backoff = true;
    static const bool silence = true;
    static const bool multichannel = false; // TODO
    static const unsigned int MICROFRAME_CHANNEL = 26; // TODO

    static const unsigned int INT_HANDLING_DELAY = 9; // Time delay between scheduled tx_mf interrupt and actual Engine TX
    static const unsigned int TX_DELAY = INT_HANDLING_DELAY + Engine::RX_TO_TX_DELAY;

    static const unsigned int Tu = IEEE802_15_4::TURNAROUND_TIME;
    static const unsigned int Ti = Tu + Engine::RX_TO_TX_DELAY + INT_HANDLING_DELAY + 100; // 100us margin for delay between Microframes // FIXME
    static const unsigned int TIME_BETWEEN_MICROFRAMES = Ti;
    static const unsigned int Ts = (sizeof(Microframe) + Phy_Layer::PHY_HEADER_SIZE) * 1000000ull
        / Phy_Layer::BYTE_RATE
        + Engine::TX_TO_RX_DELAY; // Time to send a single Microframe (including PHY headers)
    static const unsigned int MICROFRAME_TIME = Ts;
    static const unsigned int Tr = 2*Ts + Ti + (2*Ts + Ti) / 10;
    static const unsigned int RX_MF_TIMEOUT = Tr;

    static const unsigned int NMF = 1 + (((1000000ull * Tr) / Traits<System>::DUTY_CYCLE) + (Ti + Ts) - 1) / (Ti + Ts);
    static const unsigned int N_MICROFRAMES = NMF;

    static const unsigned int CI = Ts + (NMF - 1) * (Ts + Ti);
    static const unsigned int PERIOD = CI;
    static const unsigned int SLEEP_PERIOD = CI - RX_MF_TIMEOUT;

    static const typename IF<(Tr * 1000000ull / CI <= Traits<System>::DUTY_CYCLE), unsigned int, void>::Result
        DUTY_CYCLE = Tr * 1000000ull / CI; // in ppm. This line failing means that TSTP_MAC is unable to provide a duty cycle smaller than or equal to Traits<System>::DUTY_CYCLE

    // TODO
    static const unsigned int DATA_LISTEN_MARGIN = (TIME_BETWEEN_MICROFRAMES + MICROFRAME_TIME) * 2; // Subtract this amount when calculating time until data transmission
    static const unsigned int DATA_SKIP_TIME = (Phy_Layer::MTU + Phy_Layer::PHY_HEADER_SIZE) * 1000000ull / Phy_Layer::BYTE_RATE;

    static const unsigned int RX_DATA_TIMEOUT = DATA_SKIP_TIME + DATA_LISTEN_MARGIN;

    static const unsigned int G = IEEE802_15_4::CCA_TX_GAP;
    static const unsigned int CCA_TIME = Tr;

    static const unsigned int OFFSET_LOWER_BOUND = G + Engine::SLEEP_TO_RX_DELAY;
    static const unsigned int OFFSET_GENERAL_LOWER_BOUND = OFFSET_LOWER_BOUND + 3 * G;
    static const unsigned int OFFSET_UPPER_BOUND = SLEEP_PERIOD - CCA_TIME - Engine::RX_TO_TX_DELAY - MICROFRAME_TIME;
    static const unsigned int OFFSET_GENERAL_UPPER_BOUND = OFFSET_UPPER_BOUND - 3 * G;

protected:
    MAC() { db<TSTP, Init>(TRC) << "TSTP::MAC()" << endl; }

public:
    ~MAC() {
        for(Buffer::Element * el = _tx_schedule.head(); el; el = _tx_schedule.head()) {
            Buffer * buf = el->object();
            _tx_schedule.remove(el);
            if(buf)
                free(buf);
        }
    }

    void marshal(Buffer * buf, const Address & src, const Address & dst, const Type & type) {
        // Empty method, for compilation purpose only. Shall never be called if systems is correctly configured
    }

    unsigned int unmarshal(Buffer * buf, Address * src, Address * dst, Type * type, void * data, unsigned int size) {
        // Empty method, for compilation purpose only. Shall never be called if systems is correctly configured
        return 0;
    }

    void marshal(Buffer * buf) {
        buf->is_microframe = false;
        buf->trusted = false;
        buf->is_new = true;
        buf->random_backoff_exponent = 0;
        buf->microframe_count = 0;
        buf->times_txed = 0;
        buf->offset = OFFSET_GENERAL_UPPER_BOUND;
    }

    unsigned int unmarshal(Buffer * buf, void * data, unsigned int size) {
        memcpy(data, buf->frame()->data<Frame>(), (buf->size() < size ? buf->size() : size));
        return buf->size();
    }

    bool drop(Buffer * buf) {
         bool int_disabled = CPU::int_disabled();
         if(!int_disabled)
             CPU::int_disable();

         bool ret = false;
         if(_tx_schedule.remove(buf))
             ret = true;
         if(_tx_pending == buf)
             _tx_pending = 0;
         delete buf;

         if(!int_disabled)
             CPU::int_enable();

         return ret;
     }

    static bool drop(unsigned int id) {
        bool ret = false;
        bool int_disabled = CPU::int_disabled();
        if(!int_disabled)
            CPU::int_disable();
        for(Buffer::Element * el = _tx_schedule.head(); el; el = el->next()) {
            Buffer * buf = el->object();
            if(buf->id == id) {
                _tx_schedule.remove(el);
                if(_tx_pending == buf)
                    _tx_pending = 0;
                ret = true;
                delete buf;
                break;
            }
        }
        if(!int_disabled)
            CPU::int_enable();
        return ret;
    }

    int send(Buffer * buf) {
        if(sniffer) {
            delete buf;
            return 0;
        }

        buf->offset = Engine::instance()->us2count(buf->offset);

        if(buf->destined_to_me)
            buf->offset = Engine::instance()->us2count(OFFSET_LOWER_BOUND);
        else {
            if(buf->offset < Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND))
                buf->offset = Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND);
            else if(buf->offset > Engine::instance()->us2count(OFFSET_GENERAL_UPPER_BOUND))
                buf->offset = Engine::instance()->us2count(OFFSET_GENERAL_UPPER_BOUND);
        }

        // Check if we already have this message queued. If so, replace it
        CPU::int_disable();
        Buffer::Element * next;
        for(Buffer::Element * el = _tx_schedule.head(); el; el = next) {
            next = el->next();
            Buffer * queued_buf = el->object();
            if(equals(queued_buf, buf)) {
                if(_tx_pending && (_tx_pending == queued_buf))
                    _tx_pending = buf;
                _tx_schedule.remove(queued_buf->link());
                delete queued_buf;
            }
        }
        _tx_schedule.insert(buf->link());
        CPU::int_enable();

        return buf->size();
    }

private:
    // Filter and assemble RX Buffer Metadata
    bool pre_notify(Buffer * buf) {
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "pre_notify(buf=" << buf << ")" << endl;

        if(sniffer) {
            static unsigned int last_id = 0;
            //            static unsigned int last_hint = 0;
            if(buf->size() == sizeof(Microframe)) {
                Microframe * mf = buf->frame()->data<Microframe>();
//                if((mf->id() != last_id) || (mf->hint() != last_hint)) {
                    last_id = mf->id();
//                    last_hint = mf->hint();

                    buf->id = last_id;
                    buf->downlink = mf->all_listen();
                    buf->is_new = false;
                    buf->is_microframe = true;
                    buf->microframe_count = mf->count();
                    buf->offset = OFFSET_GENERAL_UPPER_BOUND;

                    // Forge a TSTP identifier to make the radio notify listeners
                    mf->all_listen(false);
                    mf->count(V0 >> 1);

//                    return true;
//                }
                return false;
            } else {
                last_id = 0;
                //                last_hint = 0;
                buf->is_microframe = false;
                return true;
            }
            return false;
        }

        if(_in_rx_mf) { // State: RX MF (part 2/3)
            // TODO: I don't know why, but some MFs with a huge count are getting here
            // TODO: check if this is still true
            if((buf->size() == sizeof(Microframe)) && (buf->frame()->data<Microframe>()->count() < N_MICROFRAMES)) {

                Engine::instance()->int_disable();

                Engine::power(Power_Mode::SLEEP);

                _in_rx_mf = false;

                Microframe * mf = buf->frame()->data<Microframe>();
                Frame_ID id = mf->id();

                // Initialize Buffer Metainformation
                buf->id = id;
                buf->downlink = mf->all_listen();
                buf->is_new = false;
                buf->is_microframe = true;
                buf->relevant = mf->all_listen();
                buf->trusted = false;
                buf->hint = mf->hint();
                buf->microframe_count = mf->count();
                buf->offset = OFFSET_GENERAL_UPPER_BOUND;
                buf->times_txed = 0;

                // Forge a TSTP identifier to make the radio notify listeners
                mf->all_listen(false);
                mf->count(V0 >> 1);

                return true;
            }
            return false;
        } else if(_in_rx_data) { // State: RX Data (part 2/3)

            if(buf->size() == sizeof(Microframe)) {
                return false;
            }

            Engine::power(Power_Mode::SLEEP);

            db<TSTP>(TRC) << "TSTP::MAC::pre_notify: Frame received: " << buf->frame() << " at " << Engine::instance()->count2us(buf->sfdts) << endl;

            // Initialize Buffer Metainformation
            buf->frame()->data<Packet>()->identify();
            buf->hint = _receiving_data_hint;
            buf->is_new = false;
            buf->is_microframe = false;
            buf->trusted = false;
            buf->random_backoff_exponent = 0;
            buf->microframe_count = 0;
            buf->offset = OFFSET_GENERAL_UPPER_BOUND;
            buf->times_txed = 0;

            // Clear scheduled messages that are equivalent
            Buffer::Element * next;
            for(Buffer::Element * el = _tx_schedule.head(); el; el = next) {
                next = el->next();
                Buffer * queued_buf = el->object();
                if(equals(queued_buf, buf)) {
                    if(!queued_buf->destined_to_me) {
                        db<TSTP>(TRC) << "TSTP::MAC::pre_notify: ACK received, ID=" << queued_buf->id << endl;
                        _tx_schedule.remove(el);
                        delete queued_buf;
                    }
                }
            }

            return true;
        } else {
            return false;
        }
    }

    bool post_notify(Buffer * buf) {
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "post_notify(buf=" << buf << ")" << endl;

        if(buf->is_microframe && !sniffer) { // State: RX MF (part 3/3)
            Time_Stamp data_time = buf->sfdts + us2ts(TIME_BETWEEN_MICROFRAMES) + buf->microframe_count * us2ts(TIME_BETWEEN_MICROFRAMES + MICROFRAME_TIME);

            // FIXME:            Watchdog::kick();
            if(state_machine_debugged)
                kout << SLEEP_DATA;

            // If we have a buffer with the same ID, we need to receive the data
            // to make sure we can clear it from the tx schedule
            if(!buf->relevant)
                for(Buffer::Element * el = _tx_schedule.head(); el; el = el->next())
                    if(el->object()->id == buf->id) {
                        buf->relevant = true;
                        break;
                    }

            if(buf->relevant) { // Transition: [Relevant MF]
                _receiving_data_hint = buf->hint;
                if(multichannel)
                    _receiving_data_channel = channel(buf);
                // State: Sleep until Data
                if(data_time > Engine::read() + us2ts(DATA_LISTEN_MARGIN / 2 + Engine::SLEEP_TO_RX_DELAY))
                    data_time -= us2ts(DATA_LISTEN_MARGIN / 2);
                else
                    data_time = Engine::instance()->read() + Engine::instance()->us2count(Engine::SLEEP_TO_RX_DELAY + 1);
                Engine::instance()->handler(data_time, rx_data);
            } else { // Transition: [Irrelevant MF]
                Engine::instance()->handler(data_time + Engine::instance()->us2count(DATA_SKIP_TIME), update_tx_schedule);
            }
        }

        free(buf);
        return true;
    }

    // Called after the Engine's constructor
    void constructor_epilogue() {
        db<TSTP, Init>(TRC) << "TSTP::MAC::constructor_epilogue()" << endl;
        if(sniffer) {
            Engine::power(Power_Mode::FULL);
            Engine::listen();
        } else {
            // Watchdog::enable();
            update_tx_schedule(0);
        }
    }

private:
    // State Machine
    static void update_tx_schedule(IC::Interrupt_Id id) {
        // State: Update TX Schedule
        CPU::int_disable();
        // Engine::instance()->int_disable();
        if(state_machine_debugged)
            kout << UPDATE_TX_SCHEDULE;
//        Watchdog::kick();
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::update_tx_schedule(id=" << id << ")" << endl;

        Engine::instance()->power(Power_Mode::SLEEP);
        _in_rx_data = false;
        _in_rx_mf = false;

        _tx_pending = 0;

        Time_Stamp now_ts = Engine::instance()->read();
        Microsecond now_us = Engine::instance()->count2us(now_ts);

        if(_silence_periods && silence)
            _silence_periods--;
        else {
            // Fetch next message and remove expired ones
            // TODO: Turn _tx_schedule into an ordered list
            for(Buffer::Element * el = _tx_schedule.head(); el; ) {
                Buffer::Element * next = el->next();
                Buffer * b = el->object();
                /*// Message was created in the future. This might happen when Timekeeper adjusts the timer.
                if(b->frame()->data<Header>()->time() > now_us) {
                    _tx_schedule.remove(el);
                    delete b;
                } else */
                // Drop expired messages
                if(drop_expired && (b->deadline <= now_us)) {
                    _tx_schedule.remove(el);
                    delete b;
                } else if(!_tx_pending) {
                    _tx_pending = b;
                    // Prioritize ACKs
                } else if(_tx_pending->destined_to_me) {
                    if(b->destined_to_me) {
                        if(b->times_txed < _tx_pending->times_txed)
                            _tx_pending = b;
                        else if((b->times_txed == _tx_pending->times_txed)
                            && (b->deadline < _tx_pending->deadline))
                            _tx_pending = b;
                    }
                    // Non-keepalives are prioritized over keepalives
                } else if((_tx_pending->frame()->data<Header>()->type() == CONTROL) && (_tx_pending->frame()->data<Header>()->subtype() == KEEP_ALIVE)) {
                    if(((b->frame()->data<Header>()->type() == CONTROL) && (b->frame()->data<Header>()->subtype() == KEEP_ALIVE))) {
                        if(b->times_txed < _tx_pending->times_txed)
                            _tx_pending = b;
                        else if((b->times_txed == _tx_pending->times_txed)
                            && (b->deadline < _tx_pending->deadline))
                            _tx_pending = b;
                    } else
                        _tx_pending = b;
                } else {
                    if(b->times_txed < _tx_pending->times_txed)
                        _tx_pending = b;
                    else if((b->times_txed == _tx_pending->times_txed)
                        && (b->deadline < _tx_pending->deadline))
                        _tx_pending = b;
                }
                el = next;
            }
        }

        if(_tx_pending) { // Transition: [TX pending]
            // State: Contend CCA (Contend part)

//            Watchdog::kick();
            if(state_machine_debugged)
                kout << CONTEND;

            Time_Stamp offset = _tx_pending->offset;
            if(random_backoff) {
                // Increase ACK priority and decrease non-ACK priority by a random component,
                // based on number of transmission attempts.
                // This prevents permanent inteference by a given pair of nodes, and
                // makes unresponded messages have the lowest priorities
                _tx_pending->random_backoff_exponent++;
                unsigned int lim = G * _tx_pending->random_backoff_exponent;
                if((lim > Engine::instance()->us2count(OFFSET_UPPER_BOUND)) || (lim == 0))
                    lim = Engine::instance()->us2count(OFFSET_UPPER_BOUND);
                if(_tx_pending->destined_to_me) {
                    offset -= ((unsigned int) (Random::random()) % lim);
                    if((offset < Engine::instance()->us2count(OFFSET_LOWER_BOUND)) || (offset > Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND))) {
                        offset = Engine::instance()->us2count(OFFSET_LOWER_BOUND);
                        _tx_pending->random_backoff_exponent--;
                    }
                } else {
                    offset += ((unsigned int) (Random::random()) % lim);

                    if(offset < Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND)) {
                        offset = Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND);
                        _tx_pending->random_backoff_exponent--;
                    } else if(offset > Engine::instance()->us2count(OFFSET_UPPER_BOUND)) {
                        offset = Engine::instance()->us2count(OFFSET_UPPER_BOUND);
                        _tx_pending->random_backoff_exponent--;
                    }
                }
            }

            bool is_model = (_tx_pending->frame()->data<Header>()->type() == CONTROL) && (_tx_pending->frame()->data<Header>()->subtype() == MODEL);

            new (&_mf) Microframe(((!_tx_pending->destined_to_me) && _tx_pending->downlink) || (is_model),
                                  _tx_pending->id, N_MICROFRAMES - 1, _tx_pending->hint);

            Engine::instance()->copy_to_nic(&_mf, sizeof(Microframe));

            // TODO: should we add the radio DELAYs ?
            Engine::instance()->handler(now_ts + offset - Engine::instance()->us2count(Engine::SLEEP_TO_RX_DELAY), cca);
            //Timer::interrupt(now_ts + offset, cca);
        } else { // Transition: [No TX pending]
            // State: Sleep S
            if(state_machine_debugged)
                kout << SLEEP_S ;
//            Watchdog::kick();
            Engine::instance()->handler(now_ts + Engine::instance()->us2count(SLEEP_PERIOD - Engine::SLEEP_TO_RX_DELAY), rx_mf);
        }
        CPU::int_enable();
    }

    // State: Contend CCA (CCA part)
    static void cca(IC::Interrupt_Id id) {
        if(state_machine_debugged)
            kout << CCA ;
//        Watchdog::kick();
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::cca(id=" << id << ")" << endl;

        assert(N_MICROFRAMES > 1);

        if(multichannel)
            Engine::instance()->channel(MICROFRAME_CHANNEL);
        Engine::instance()->power(FULL);
        Engine::instance()->listen();

        // Try to send the first Microframe
        if(Engine::instance()->cca(CCA_TIME)) {
            _mf_time = Engine::instance()->read() + Engine::instance()->us2count(Engine::RX_TO_TX_DELAY);
            if(Engine::instance()->transmit()) { // Transition: [Channel free]
                _mf_time += Engine::instance()->us2count(TIME_BETWEEN_MICROFRAMES + MICROFRAME_TIME);
                _mf.dec_count();
                if(state_machine_debugged)
                    kout << TX_MF ;
//                Watchdog::kick();
                while(!Engine::instance()->tx_done());
                Engine::instance()->copy_to_nic(&_mf, sizeof(Microframe));
                //tx_mf(0);
                Engine::instance()->handler(_mf_time - Engine::instance()->us2count(TX_DELAY), tx_mf);
            } else { // Transition: [Channel busy]
                rx_mf(0);
            }
        } else { // Transition: [Channel busy]
            rx_mf(0);
        }
    }

    // State: RX MF (part 1/3)
    static void rx_mf(IC::Interrupt_Id id) {
        if(state_machine_debugged)
            kout << RX_MF ;
//        Watchdog::kick();
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::rx_mf(id=" << id << ")" << endl;

        _in_rx_data = false;
        _in_rx_mf = true;

        // If timeout is reached, Transition: [No MF]
        // TODO: radio DELAYs
        Engine::instance()->handler(Engine::instance()->read() + Engine::instance()->us2count(RX_MF_TIMEOUT), update_tx_schedule);

        //if(multichannel && radio_sleeping)
        //    Engine::channel(MICROFRAME_CHANNEL);
        Engine::instance()->power(Power_Mode::FULL);
        Engine::instance()->listen();
    }

    // State: RX Data (part 1/3)
    static void rx_data(IC::Interrupt_Id id) {
        if(state_machine_debugged)
            kout << RX_DATA ;
// FIXME:        Watchdog::kick();
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::rx_data(id=" << id << ")" << endl;

        _in_rx_data = true;
        _in_rx_mf = false;

        // Set timeout
        Engine::instance()->handler(Engine::instance()->read() + Engine::instance()->us2count(RX_DATA_TIMEOUT), update_tx_schedule);

        if(multichannel)
            Engine::instance()->channel(_receiving_data_channel);
        Engine::instance()->power(Power_Mode::FULL);
        Engine::instance()->listen();
    }

    // State: TX MFs
    static void tx_mf(IC::Interrupt_Id id) {
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::tx_mf()" << endl;

        // The first Microframe is sent at cca()
        Engine::instance()->transmit_no_cca();
        _mf_time += Engine::instance()->us2count(TIME_BETWEEN_MICROFRAMES + MICROFRAME_TIME);
//        Watchdog::kick();

        if(_mf.dec_count() > 0) {
            while(!Engine::instance()->tx_done());
            Engine::instance()->copy_to_nic(&_mf, sizeof(Microframe));
            Engine::instance()->handler(_mf_time - Engine::instance()->us2count(TX_DELAY), tx_mf);
        } else {
            CPU::int_disable();
            // The precise time when this frame is actually sent by the physical layer
            _tx_pending->frame()->data<Header>()->last_hop(_mf_time + Engine::instance()->us2count(TX_DELAY + Tu));
            while(!Engine::instance()->tx_done());
            Engine::instance()->copy_to_nic(_tx_pending->frame(), _tx_pending->size());
            Engine::instance()->handler(_mf_time - Engine::instance()->us2count(TX_DELAY), tx_data);
            if(multichannel)
                Engine::instance()->channel(channel(_tx_pending));
            CPU::int_enable();
        }
    }

    static void tx_data(IC::Interrupt_Id id) {
        Engine * _engine = Engine::instance();
        if(state_machine_debugged)
            kout << TX_DATA;
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP_MAC::tx_data()" << endl;

        // State: TX Data
        CPU::int_disable();
        if(_tx_pending) { // _tx_pending might have been deleted at drop()
            _engine->transmit_no_cca();
//            Watchdog::kick();

            bool is_keep_alive = (_tx_pending->frame()->data<Header>()->type() == CONTROL) && (_tx_pending->frame()->data<Header>()->subtype() == KEEP_ALIVE);

            _tx_pending->times_txed++;

            if(silence && !is_keep_alive && !_tx_pending->destined_to_me) {
                unsigned int r = Random::random();
                _silence_periods += (r % _tx_pending->times_txed) + 1;
            }

            while(!_engine->tx_done());

            _mf_time = _engine->read();

            // Keep Alive messages are never ACK'ed or forwarded
            if(is_keep_alive || _tx_pending->destined_to_me) {
                _tx_schedule.remove(_tx_pending->link());
                delete _tx_pending;
                _tx_pending = 0;
            }
        } else {
            _mf_time = _engine->read();
        }
        CPU::int_enable();

        // State: Sleep S
        // _engine->power(Power_Mode::SLEEP);
        if(state_machine_debugged)
            kout << SLEEP_S;
        if(multichannel)
            _engine->channel(MICROFRAME_CHANNEL);
        // TODO: Engine DELAYs
        _engine->handler(_mf_time + Engine::instance()->us2count(SLEEP_PERIOD), rx_mf);
    }

    static unsigned int channel(Buffer * b) {
        return (b->id % 14) + 11;
    }

    void free(Buffer * b);
    bool equals(Buffer *, Buffer *);

    static Microframe _mf;
    static Time_Stamp _mf_time;
    static Hint _receiving_data_hint;
    static Buffer::List _tx_schedule;
    static Buffer * _tx_pending;
    static bool _in_rx_mf;
    static bool _in_rx_data;
    static unsigned int _receiving_data_channel;
    static unsigned int _silence_periods;

    unsigned int _unit;

    static MAC<Engine, true> instance();

};

// The compiler makes sure that template static variables are only defined once

// Class attributes
template<typename Engine>
typename TSTP::MAC<Engine, true>::Microframe TSTP::MAC<Engine, true>::_mf;

template<typename Engine>
typename TSTP::MAC<Engine, true>::Time_Stamp TSTP::MAC<Engine, true>::_mf_time;

template<typename Engine>
typename TSTP::MAC<Engine, true>::Hint TSTP::MAC<Engine, true>::_receiving_data_hint;

template<typename Engine>
typename TSTP::Buffer::List TSTP::MAC<Engine, true>::_tx_schedule;

template<typename Engine>
typename TSTP::Buffer * TSTP::MAC<Engine, true>::_tx_pending;

template<typename Engine>
bool TSTP::MAC<Engine, true>::_in_rx_mf;

template<typename Engine>
bool TSTP::MAC<Engine, true>::_in_rx_data;

template<typename Engine>
unsigned int TSTP::MAC<Engine, true>::_receiving_data_channel;

template<typename Engine>
unsigned int TSTP::MAC<Engine, true>::_silence_periods;


// TSTP::MAC without duty cycling and without microframes
template<typename Engine>
class TSTP::MAC<Engine, false>: public Engine
{
protected:
    typedef CPU::Reg16 Frame_ID;
    typedef CPU::Reg32 Hint;
    typedef CPU::Reg16 MF_Count;
    typedef NIC_Common::CRC16 CRC;
    typedef IEEE802_15_4 Phy_Layer;
    static TSTP::MAC<Engine, false> _instance;

public:
    enum State {
        UPDATE_TX_SCHEDULE = 0,
        RX                 = 1,
        CCA_TX             = 2,
    };

    typedef typename Engine::Timer Timer;
    typedef typename Engine::Timer::Time_Stamp Time_Stamp;
    typedef _UTIL::Buffer<NIC<IEEE802_15_4>, Raw_Frame, void, TSTP_Metadata> Buffer; // here Buffer MUST be the TSTP one

    static const unsigned int MTU = Raw_Frame::MTU;

protected:
    static const bool sniffer = Traits<Traits<IEEE802_15_4>::DEVICES::Get<0>::Result>::promiscuous;
    static const bool state_machine_debugged = false;
    static const bool random_backoff = true;
    static const unsigned int PERIOD = 150000; // TODO
    static const unsigned int OFFSET_LOWER_BOUND = 2 * IEEE802_15_4::CCA_TX_GAP;
    static const unsigned int OFFSET_UPPER_BOUND = PERIOD;
    static const unsigned int OFFSET_GENERAL_UPPER_BOUND = OFFSET_UPPER_BOUND - 2 * IEEE802_15_4::CCA_TX_GAP;
    static const unsigned int OFFSET_GENERAL_LOWER_BOUND = 2 * OFFSET_LOWER_BOUND;
    static const unsigned int CCA_TIME = IEEE802_15_4::CCA_TX_GAP;
    static const unsigned int RX_TO_TX_DELAY = IEEE802_15_4::TURNAROUND_TIME + Engine::RX_TO_TX_DELAY;

protected:
    MAC() { db<TSTP, Init>(TRC) << "TSTP::MAC()" << endl; }

    // Called after the Engine's constructor
    void constructor_epilogue() {
        db<TSTP, Init>(TRC) << "TSTP::MAC_NOMF::constructor_epilogue()" << endl;
        Engine::instance()->power(Power_Mode::FULL);
        Engine::instance()->listen(); // Engine is assumed to always return to RX after TX
        if(!sniffer)
            update_tx_schedule(0);
    }

public:
    static unsigned int period() { return PERIOD; }

    // Assemble TX Buffer Metainformation
    void marshal(Buffer * buf) {
        buf->id = 0;
        buf->is_microframe = false;
        buf->trusted = false;
        buf->is_new = true;
        buf->random_backoff_exponent = 0;
        buf->microframe_count = 0;
        buf->offset = OFFSET_GENERAL_UPPER_BOUND;
    }

    unsigned int unmarshal(Buffer * buf, void * data, unsigned int size) {
        memcpy(data, buf->frame()->data<Frame>(), (buf->size() < size ? buf->size() : size));
        return buf->size();
    }

    void marshal(Buffer * buf, const Address & src, const Address & dst, const IEEE802_15_4::Type  & type) {
    }

    unsigned int unmarshal(Buffer * buf, Address * src, Address * dst, IEEE802_15_4::Type  * type, void * data, unsigned int size) {
        return 0;
    }

    bool drop(Buffer * buf) {
        return drop(buf->id);
    }

    bool drop(unsigned int id) {
        bool ret = false;
        bool int_disabled = CPU::int_disabled();
        if(!int_disabled)
            CPU::int_disable();
        for(Buffer::Element * el = _tx_schedule.head(); el; el = el->next()) {
            Buffer * buf = el->object();
            if(buf->id == id) {
                _tx_schedule.remove(el);
                if(_tx_pending == buf)
                    _tx_pending = 0;
                ret = true;
                delete buf;
                break;
            }
        }
        if(!int_disabled)
            CPU::int_enable();
        return ret;
    }

    int send(Buffer * buf) {
        if(sniffer) {
            delete buf;
            return 0;
        }

        db<TSTP>(TRC) << "TSTP_MAC_NOMF::send(b=" << buf << ")" << endl;
        if(buf->destined_to_me)
            buf->offset = Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND);
        else {
            buf->offset = Engine::instance()->us2count(buf->offset);

            if(buf->offset < Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND))
                buf->offset = Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND);
            else if(buf->offset > Engine::instance()->us2count(OFFSET_GENERAL_UPPER_BOUND))
                buf->offset = Engine::instance()->us2count(OFFSET_GENERAL_UPPER_BOUND);
        }

        // Check if we have a message with the same ID. If so, replace it
        Buffer::Element * next;
        for(Buffer::Element * el = _tx_schedule.head(); el; el = next) {
            next = el->next();
            Buffer * queued_buf = el->object();
            if(equals(queued_buf, buf)) {
                if(_tx_pending && (_tx_pending == queued_buf))
                    _tx_pending = buf;
                _tx_schedule.remove(queued_buf->link());
                delete queued_buf;
                db<TSTP>(TRC) << "TSTP_MAC_NOMF::send(b=" << buf << ") => deleted buffer with equivalent message" << endl;
            }
        }

        _tx_schedule.insert(buf->link());

        //if(!_notifying)
        update_tx_schedule(0); // implicit int_enable
        // else, int_enable at post_notify

        return buf->size();
    }

protected:
    bool pre_notify(Buffer * buf) {
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "TSTP::MAC_NOMF::pre_notify(b=" << buf << endl;

        _notifying = true;

        _stats.rx_packets++;

        // Initialize Buffer Metainformation
        buf->is_new = false;
        buf->is_microframe = false;
        buf->trusted = false;
        buf->random_backoff_exponent = 0;
        buf->relevant = false;
        buf->offset = OFFSET_GENERAL_UPPER_BOUND;
        buf->frame()->data<Packet>()->identify();

        // Clear scheduled messages with same ID
        Buffer::Element * next;
        for(Buffer::Element * el = _tx_schedule.head(); el; el = next) {
            next = el->next();
            Buffer * queued_buf = el->object();
            if(queued_buf->id == buf->id) {
                if(!queued_buf->destined_to_me) {
                    db<TSTP>(TRC) << "TSTP::MAC_NOMF::pre_notify: ACK received" << endl;
                    if(queued_buf == _tx_pending) {
                        // Engine::instance()->int_disable();
                        _tx_pending = 0;
                    }
                    _tx_schedule.remove(el);
                    delete queued_buf;
                }
            }
        }

        // int_enable at post_notify
        return true;
    }

    bool post_notify(Buffer * buf) {
        free(buf);
        _notifying = false;
        if(!sniffer)
            update_tx_schedule(0);
        return true;
    }

    bool equals(Buffer *, Buffer *);

private:
    // State Machine

    static void update_tx_schedule(IC::Interrupt_Id id) {
        if(state_machine_debugged)
            kout << UPDATE_TX_SCHEDULE;
        if(Traits<TSTP>::hysterically_debugged)
            db<TSTP>(TRC) << "State: Update TX Schedule" << endl;

        Buffer * tx_already_pending = _tx_pending;


        Time_Stamp now_ts = Engine::instance()->time_stamp();
        Microsecond now_us = 999999999UL;
	// should not, but sometimes is called with null _nic
        if (_nic != 0 ) {
        	now_us = ts2us( now_ts );
        }

        // Fetch next message and remove expired ones
        // TODO: Turn _tx_schedule into an ordered list
        for(Buffer::Element * el = _tx_schedule.head(); el; ) {
            Buffer::Element * next = el->next();
            Buffer * b = el->object();
            /*
            // Message was created in the future. This might happen when Timekeeper adjusts the timer.
            if(b->frame()->data<Header>()->time() > now_us) {
            if(b == _tx_pending) {
            Timer::int_disable();
            _tx_pending = 0;
            }
            _tx_schedule.remove(el);
            delete b;
            } else */
            if(drop_expired && (b->deadline <= now_us)) {
                if(b == _tx_pending) {
                    Engine::instance()->int_disable();
                    _tx_pending = 0;
                }
                _tx_schedule.remove(el);
                delete b;
            } else if(!_tx_pending) {
                _tx_pending = b;
            } else if(_tx_pending->destined_to_me) {
                if(b->destined_to_me) {
                    if(b->random_backoff_exponent < _tx_pending->random_backoff_exponent) {
                        _tx_pending = b;
                    } else if((b->random_backoff_exponent == _tx_pending->random_backoff_exponent) && (b->deadline < _tx_pending->deadline)) {
                        _tx_pending = b;
                    }
                }
            } else {
                if(b->random_backoff_exponent < _tx_pending->random_backoff_exponent) {
                    _tx_pending = b;
                } else if((b->random_backoff_exponent == _tx_pending->random_backoff_exponent) && (b->deadline < _tx_pending->deadline)) {
                    _tx_pending = b;
                }
            }
            el = next;
        }

        if(_tx_pending && (_tx_pending != tx_already_pending)) { // Transition: [TX pending]
            if(Traits<TSTP>::hysterically_debugged)
                db<TSTP>(TRC) << "Transition: [TX pending]" << endl;

            Time_Stamp offset = _tx_pending->offset;
            if(random_backoff) {
                // Increase ACK priority and decrease non-ACK priority by a random component,
                // based on number of transmission attempts.
                // This prevents permanent inteference by a given pair of nodes, and
                // makes unresponded messages have the lowest priorities
                unsigned int lim = Engine::instance()->us2count(CCA_TX_GAP) << _tx_pending->random_backoff_exponent;
                if(!lim)
                    lim = Engine::instance()->us2count(OFFSET_UPPER_BOUND);
                if(_tx_pending->destined_to_me) {
                    offset -= ((unsigned int)(Random::random()) % lim);// * Engine::instance()->us2count(G);
                    if((offset < Engine::instance()->us2count(OFFSET_LOWER_BOUND)) || (offset > Engine::instance()->us2count(OFFSET_UPPER_BOUND))) {
                        offset = Engine::instance()->us2count(OFFSET_LOWER_BOUND);
                    }
                } else {
                    offset += ((unsigned int)(Random::random()) % lim);// * Engine::instance()->us2count(G);
                    if(offset < Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND)) {
                        offset = Engine::instance()->us2count(OFFSET_GENERAL_LOWER_BOUND);
                    } else if(offset > Engine::instance()->us2count(OFFSET_UPPER_BOUND)) {
                        offset = Engine::instance()->us2count(OFFSET_UPPER_BOUND);
                    }
                }
            }

            // The precise time when this frame is actually sent by the physical layer
            _tx_pending->frame()->data<Header>()->last_hop(now_ts + offset + Engine::instance()->us2count(Engine::RX_TO_TX_DELAY + CCA_TIME));
            Engine::instance()->copy_to_nic(_tx_pending->frame(), _tx_pending->size());
            Engine::instance()->handler(now_ts + Engine::instance()->us2count(offset), cca_tx);
        }
    }

    static void cca_tx(IC::Interrupt_Id id) {
        if(_tx_pending) { // _tx_pending might have been deleted by drop()
            Engine::instance()->copy_to_nic(_tx_pending->frame(), _tx_pending->size());
            if(Engine::instance()->cca(CCA_TIME)) {
                Engine::instance()->transmit();
                _stats.tx_packets++;
                _stats.tx_bytes += _tx_pending->size();
                if(!_tx_pending->is_new)
                    _stats.tx_relayed++;

                if(_tx_pending->destined_to_me || ((_tx_pending->frame()->data<Header>()->type() == CONTROL) && (_tx_pending->frame()->frame()->data<Header>()->subtype() == KEEP_ALIVE))) {
                    _tx_schedule.remove(_tx_pending->link());
                    delete _tx_pending;
                } else
                    _tx_pending->random_backoff_exponent++;
                _tx_pending = 0;
                while(!Engine::instance()->tx_done());
            }
            else {
                _tx_pending->random_backoff_exponent++;
                _tx_pending = 0;
            }
        }
        Engine::instance()->handler(Engine::instance()->read() + Engine::instance()->us2count(PERIOD), update_tx_schedule);
    }

    static MAC<Engine, false> instance();

private:
    void free(Buffer * b);

private:
    static Buffer::List _tx_schedule;
    static Buffer * _tx_pending;
    static Statistics _stats;
    static bool _notifying;
};

template<typename Engine>
typename TSTP::Buffer::List TSTP::MAC<Engine, false>::_tx_schedule;

template<typename Engine>
typename TSTP::Buffer * TSTP::MAC<Engine, false>::_tx_pending;

template<typename Engine>
TSTP::Statistics TSTP::MAC<Engine, false>::_stats;

template<typename Engine>
bool TSTP::MAC<Engine, false>::_notifying;

template<typename Engine>
bool TSTP::MAC<Engine, true>::equals(Buffer * b0, Buffer * b1)
{
    if(b0->id != b1->id)
        return false;

    Header * h0 = b0->frame()->data<Header>();
    Header * h1 = b1->frame()->data<Header>();
    return (h1->version() == h0->version()) && (h1->type() == h0->type()) && (h1->scale() == h0->scale()) && (h1->time() == h0->time()) && (h1->origin() == h0->origin());
}

template<typename Engine>
bool TSTP::MAC<Engine, false>::equals(Buffer * b0, Buffer * b1)
{
    if(b0->id != b1->id)
        return false;

    Header * h0 = b0->frame()->data<Header>();
    Header * h1 = b1->frame()->data<Header>();
    return (h1->version() == h0->version()) && (h1->type() == h0->type()) && (h1->scale() == h0->scale()) && (h1->time() == h0->time()) && (h1->origin() == h0->origin());
}


__END_SYS

#endif

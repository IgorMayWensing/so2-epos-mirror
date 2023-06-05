// EPOS IEEE 802.15.4 MAC Declarations

#ifndef __ieee802_15_4_mac_h
#define __ieee802_15_4_mac_h

#include <utility/random.h>
#include <utility/math.h>
#include <network/ieee802_15_4.h>

#ifdef __ieee802_15_4__

__BEGIN_SYS

template<typename Engine>
class IEEE802_15_4_MAC: public IEEE802_15_4, public Engine
{
private:
    static const unsigned int CSMA_CA_MIN_BACKOFF_EXPONENT = 3;
    static const unsigned int CSMA_CA_MAX_BACKOFF_EXPONENT = 5;
    static const unsigned int CSMA_CA_UNIT_BACKOFF_PERIOD = IEEE802_15_4::CCA_TX_GAP; // us
    static const unsigned int CSMA_CA_RETRIES = Traits<Network>::RETRIES > 4 ? 4 : Traits<Network>::RETRIES;

    static const unsigned int ACK_TIMEOUT = 352 * 2;

    static const bool acknowledged = true;

public:
    using Frame = Log_Frame;
    using typename Engine::Timer;

protected:
    IEEE802_15_4_MAC() {}

public:
    // Called after the Engine's constructor
    void constructor_epilogue() {
        Engine::power(Power_Mode::FULL);
        Engine::listen();
    }

    // Assemble TX Buffer Metadata and MAC Header
    void marshal(Buffer * buf, const Address & src, const Address & dst, const Type & type) {
        Frame * frame = new (buf->frame()) Frame(type, src, dst);
        frame->ack_request(acknowledged && dst != broadcast());
    }

    unsigned int unmarshal(Buffer * buf, Address * src, Address * dst, Type * type, void * data, unsigned int size) {
        Buffer::Frame * frame = buf->frame();
        unsigned int data_size = buf->size() - sizeof(Header);
        if(size > data_size)
            size = data_size;
        *src = frame->src();
        *dst = frame->dst();
        *type = frame->type();
        memcpy(data, frame->data<void>(), size);
        return size;
    }

    void marshal(Buffer * buf) {
        // Empty method, for compilation purpose only. Shall never be called if systems is correctly configured
    }

    unsigned int unmarshal(Buffer * buf, void * data, unsigned int size) {
        // Empty method, for compilation purpose only. Shall never be called if systems is correctly configured
        return 0;
    }

    bool drop(Buffer * buf) {
        // Empty method, for compilation purpose only. Should never be called if systems is correctly configured
        return false;
    }

    int send(Buffer * buf) {
        bool do_ack = acknowledged && buf->frame()->ack_request();

        Engine::power(Power_Mode::LIGHT);
        Engine::copy_to_nic(buf->frame(), buf->size());
        bool sent, ack_ok;
        ack_ok = sent = backoff_and_send();
        if(do_ack) {
            if(sent) {
                Engine::power(Power_Mode::FULL);
                ack_ok = Engine::wait_for_ack(ACK_TIMEOUT, buf->frame()->sequence_number());
            }

            for(unsigned int i = 0; !ack_ok && (i < CSMA_CA_RETRIES); i++) {
                Engine::power(Power_Mode::LIGHT);
                ack_ok = sent = backoff_and_send();
                if(sent) {
                    Engine::power(Power_Mode::FULL);
                    ack_ok = Engine::wait_for_ack(ACK_TIMEOUT, buf->frame()->sequence_number());
                }
            }
            if(!sent)
                Engine::power(Power_Mode::FULL);

        } else {
            if(sent)
                while(!Engine::tx_done());
            Engine::power(Power_Mode::FULL);
        }

        unsigned int size = buf->size();

        delete buf;

        return ack_ok ? size : 0;
    }

    static unsigned int period() { return 0; }

    // Filter and assemble RX Buffer Metadata
    bool pre_notify(Buffer * buf) { return true; }
    bool post_notify(Buffer * buf) { return false; }

    bool backoff_and_send() {
        unsigned int exp = CSMA_CA_MIN_BACKOFF_EXPONENT;
        unsigned int backoff = Math::pow(2, exp);

        unsigned int retry = 0;
        for(; (retry < CSMA_CA_RETRIES) ; retry++) {
            unsigned int time = (Random::random() % backoff) * CSMA_CA_UNIT_BACKOFF_PERIOD;
            if(time < CSMA_CA_UNIT_BACKOFF_PERIOD)
                time = CSMA_CA_UNIT_BACKOFF_PERIOD;

            Engine::backoff(time);
            if(Engine::cca(CSMA_CA_UNIT_BACKOFF_PERIOD) && Engine::transmit())
                break; // Success

            if(exp < CSMA_CA_MAX_BACKOFF_EXPONENT) {
                exp++;
                backoff *= 2;
            }
        }
        return (retry < CSMA_CA_RETRIES);
    }
};

__END_SYS

#endif

#endif

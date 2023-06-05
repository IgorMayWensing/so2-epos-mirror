#include <network.h>
#include <time.h>
#include <synchronizer.h>
#include <utility/observer.h>
#include <utility/convert.h>
#include <utility/pcap.h>

__BEGIN_SYS

class TSTP_PCAP_Sniffer: private Data_Observer<TSTP::Buffer>
{
    typedef IF<Traits<USB>::enabled, USB, UART>::Result Out;

public:
    class Packet
    {
    public:
        typedef Simple_List<Packet> List;
        typedef List::Element Element;

        Packet(TSTP::Buffer * buf):
//            _header(TSTP::absolute(Convert::count2us(buf->sfdts)), buf->size()),
            _link(this)
        {
//            if(buf->is_microframe) {
//                TSTP::Microframe * mf = buf->frame()->data<TSTP::Microframe>();
//                mf->all_listen(buf->downlink);
//                mf->count(buf->microframe_count);
//            }
            memcpy(_data, buf->frame()->data<const char>(), buf->size());
        }

        Element * lext() { return &_link; }
        unsigned int size() { return sizeof(PCAP::Packet_Header) + _header.size(); }

    private:
        PCAP::Packet_Header _header;
        unsigned char _data[IEEE802_15_4::MTU];
        Element _link;
    };

private:
    typedef Concurrent_Observer<Packet, int> Observer;
    typedef Concurrent_Observed<Packet, int> Observed;

public:
    TSTP_PCAP_Sniffer() {
        Out out(1);
        PCAP::Global_Header g(IEEE802_15_4::MTU, PCAP::IEEE802_15_4);
        for(unsigned int i = 0; i < sizeof(PCAP::Global_Header); i++)
            out.put(reinterpret_cast<const char*>(&g)[i]);
        attach(&_observer, PCAP::IEEE802_15_4);
        TSTP::attach(this);
    }

    Packet * updated() { return _observer.updated(); }

    void update(Data_Observed<TSTP::Buffer> * obs, TSTP::Buffer * buf) { notify(PCAP::IEEE802_15_4, new Packet(buf)); }

private:
    static bool notify(int t, Packet * p) { return _observed.notify(t, p); }
    static void attach(Observer * obs, int t) { _observed.attach(obs, t); }
    static void detach(Observer * obs, int t) { _observed.detach(obs, t); }

private:
    Observer _observer;
    static Observed _observed;
};

TSTP_PCAP_Sniffer::Observed TSTP_PCAP_Sniffer::_observed;

__END_SYS

using namespace EPOS;

int main()
{
    Machine::delay(5000000);

    TSTP_PCAP_Sniffer sniffer;
    IF<Traits<USB>::enabled, USB, UART>::Result out(0);

    while(true) {
        TSTP_PCAP_Sniffer::Packet * p = sniffer.updated();
        for(unsigned int i = 0; i < p->size(); i++)
            out.put(reinterpret_cast<const char*>(p)[i]);
        delete p;
    }

    return 0;
}

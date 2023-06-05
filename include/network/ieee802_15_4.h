// EPOS IEEE 802.15.4 Mediator Common Package

#include <system/config.h>

#ifdef __ieee802_15_4__

#ifndef __ieee802_15_4_h
#define __ieee802_15_4_h

#include <architecture/cpu.h>
#define __nic_common_only__
#include <machine/nic.h>
#undef __nic_common_only__
#include <utility/list.h>
#include <utility/observer.h>
#include <utility/buffer.h>
#include <utility/random.h>
__BEGIN_SYS

class IEEE802_15_4: public NIC_Common
{
private:
    typedef CPU::Reg8 Reg8;
    typedef CPU::Reg16 Reg16;

public:
    // IEEE 802.15.4 Physical Layer
    static const unsigned int PHY_MTU = 128;
    static const unsigned int CCA_TX_GAP = 160;
    static const unsigned int TURNAROUND_TIME = 192;
    static const unsigned int BYTE_RATE = 31250; // bytes per second
    static const unsigned int SHR_SIZE = 5; // bytes
    static const unsigned int PHR_SIZE = 1; // bytes
    static const unsigned int PHY_HEADER_SIZE = SHR_SIZE + PHR_SIZE; // bytes

    typedef unsigned char Phy_Header; // PSDU length

    // The Phy_Header in IEEE 802.15.4 is just the data length and it is usually added by the NIC automatically,
    // so it Phy_Frame does not account for it explicitly
    class Phy_Frame
    {
    public:
        static const unsigned int MTU = PHY_MTU - sizeof(Phy_Header);
        typedef unsigned char Data[MTU];

    public:
        Phy_Frame() {}

        template<typename T>
        T * data() { return reinterpret_cast<T *>(&_data); }

        friend Debug & operator<<(Debug & db, const Phy_Frame & f) {
            db << "{d=" << f._data << "}";
            return db;
        }

    protected:
        Data _data;
    } __attribute__((packed));


    // IEEE 802.15.4 common MAC definitions
    typedef unsigned char Type;
    enum
    {
        BEACON  = 0,
        DATA    = 1,
        ACK     = 2,
        MAC_CMD = 3,
        EXP     = 4 // experimental, not standard
    };

    enum Version
    {
        IEEE802_15_4_2003 = 0,
        IEEE802_15_4_2006 = 1,
    };

    enum Addressing_Mode
    {
        NO_ADDRRESS      = 0,
        SHORT_ADDRESS    = 2,
        EXTENTED_ADDRESS = 4,
    };

    class Frame_Control
    {
    public:
        Frame_Control(unsigned short type = DATA, bool se = 0, bool fp = 0, bool ar = 0, bool pic = 1, unsigned short dam = NO_ADDRRESS, unsigned short fv = Version::IEEE802_15_4_2006, unsigned short sam = NO_ADDRRESS)
        : _fc(type | se << 3 | fp << 4 | ar << 5 | pic << 6 | dam << 10 | fv << 12 | sam << 14) {}

        unsigned short type() const { return _fc & 0x0007; }
        void type(unsigned short t) { _fc = (_fc & (~0x0007)) | (t & 0x0007); }

        bool se() const { return (_fc >> 3) & 0x0001; }
        void se(bool b) { _fc = (_fc & (~(1 << 3))) | (b << 3); }

        bool fp() const { return (_fc >> 4) & 0x0001; }
        void fp(bool b) { _fc = (_fc & (~(1 << 4))) | (b << 4); }

        bool ar() const { return (_fc >> 5) & 0x0001; }
        void ar(bool b) { _fc = (_fc & (~(1 << 5))) | (b << 5); }

        bool pic() const { return (_fc >> 6) & 0x0001; }
        void pic(bool b) { _fc = (_fc & (~(1 << 6))) | (b << 6); }

        unsigned short dam() const { return (_fc >> 10) & 0x0003; }
        void dam(unsigned short m) { _fc = (_fc & (~(3 << 10))) | ((m & 0x0003) << 10); }

        unsigned short fv() const { return (_fc >> 12) & 0x0003; }
        void fv(unsigned short v) { _fc = (_fc & (~(3 << 12))) | ((v & 0x0003) << 12); }

        unsigned short sam() const { return (_fc & 0x0003); }
        void sam(unsigned short m) { _fc = (_fc & (~(3 << 14))) | ((m & 0x0003) << 14); }

        friend Debug & operator<<(Debug & db, const Frame_Control & fc) {
            db << "{type=" << fc.type() << ",se=" << fc.se() << ",fp=" << fc.fp() << ",ar=" << fc.ar() << ",pic=" << fc.pic() << ",dam=" << fc.dam() << ",fv=" << fc.fv() << ",sam=" << fc.sam() << "}";
            return db;
        }

    private:
        unsigned short _fc;
    } __attribute__((packed));

    typedef NIC_Common::Address<2> Short_Address;
    typedef NIC_Common::Address<8> Extended_Address;
    typedef Short_Address Address;
    typedef NIC_Common::CRC16 Trailer;


    // ELP MAC Layer for IEEE 802.15.4
    typedef unsigned short Frame_Id;

    class Raw_Header
    {
    public:
        Raw_Header(): _fc(EXP, 0, 0, 0, 1, NO_ADDRRESS, Version::IEEE802_15_4_2006, NO_ADDRRESS) {};

        void sequence_number(const Frame_Id & id) { _id = id; }
        const Frame_Id & sequence_number() { return _id; }

        bool pending() const { return _fc.fp(); }
        unsigned int type() const { return _fc.type(); }

        void ack_request(bool val) { _fc.ar(val); }
        bool ack_request() { return _fc.ar(); }

        friend Debug & operator<<(Debug & db, const Raw_Header & h) {
            db << "{" << "fc=" << h._fc << ",id=" << h._id << "}";
            return db;
        }

    protected:
        Frame_Control _fc;
        Frame_Id _id;
    };

    class Raw_Frame: public Raw_Header
    {
    public:
        static const unsigned int MTU = PHY_MTU - sizeof(Phy_Header) - sizeof(Trailer);
        typedef unsigned char Data[MTU];

    public:
        Raw_Frame() {}
        Raw_Frame(const void * data, unsigned int size) {
            assert(size <= MTU);
            memcpy(_data, data, size);
        }

        Raw_Header * header() { return this; }

        const Address src() const { return Address::NULL; }
        const Address dst() const { return Address::NULL; }

        template<typename T>
        T * data() { return reinterpret_cast<T *>(&_data); }

        friend Debug & operator<<(Debug & db, const Raw_Frame & f) {
            db << "{d=" << f._data << ",c=" << f._crc << "}";
            return db;
        }

    protected:
        Data _data;
        Trailer _crc; // CRC is usually automatically handled by the NIC, but it is visible
    } __attribute__((packed));


    // IEEE 802.15.4 MAC Layer
    typedef Type Protocol;
    enum
    {
        PROTO_IP     = 1,
        PROTO_ARP    = 1,
        PROTO_RARP   = 1,
        PROTO_TSTP   = 4,
        PROTO_ELP    = 4,
        PROTO_PTP    = 1
    };

    enum
    {
        PAN_ID_BROADCAST = 0xffff
    };

    // Header
    // IEEE 802.15.4 MAC headers can have variable format, for now this only supports a simple, fixed format
    class Log_Header
    {
    public:
        Log_Header() {}
        Log_Header(const Type & type)
        : _frame_control(type), _sequence_number(Random::random()), _dst_pan_id(PAN_ID_BROADCAST) {};
        Log_Header(const Type & type, const Address & src, const Address & dst)
        : _frame_control(type), _sequence_number(Random::random()), _dst_pan_id(PAN_ID_BROADCAST), _dst(dst), _src(src) { ack_request(dst != broadcast()); }

        const Address & src() const { return _src; }
        const Address & dst() const { return _dst; }

        void sequence_number(Reg8 seq) { _sequence_number = seq; }
        const Reg8 & sequence_number() { return _sequence_number; }

        bool pending() const { return _frame_control.fp(); }
        unsigned int type() const { return _frame_control.type(); }

        void ack_request(bool val) { _frame_control.ar(val); }
        bool ack_request() { return _frame_control.ar(); }

        friend Debug & operator<<(Debug & db, const Log_Header & h) {
            db << "{" << "fc=" << h._frame_control << ",sn=" << h._sequence_number << ",pid=" << h._dst_pan_id << ",dst=" << h._dst << ",src=" << h._src << "," << "}";
            return db;
        }

    protected:
        Frame_Control _frame_control;
        Reg8 _sequence_number;
        Reg16 _dst_pan_id;
        Address _dst;
        Address _src;
    } __attribute__((packed));


    class Log_Frame: public Log_Header
    {
    public:
        static const unsigned int MTU = PHY_MTU - sizeof(Phy_Header) - sizeof(Log_Header) - sizeof(Trailer);
        typedef unsigned char Data[MTU];

    public:
        Log_Frame() {}
        Log_Frame(const Type & type, const Address & src, const Address & dst): Log_Header(type, src, dst) {}
        Log_Frame(const Type & type, const Address & src, const Address & dst, const void * data, unsigned int size)
        : Log_Header(type, src, dst) {
            assert(size <= MTU);
            memcpy(_data, data, size);
        }

        Log_Header * header() { return this; }

        template<typename T>
        T * data() { return reinterpret_cast<T *>(&_data); }

        friend Debug & operator<<(Debug & db, const Log_Frame & f) {
            db << "{h=" << reinterpret_cast<const Log_Header &>(f) << ",d=" << f._data << ",c=" << f._crc << "}";
            return db;
        }

    protected:
        Data _data;
        Trailer _crc; // CRC is usually automatically handled by the NIC, but it is visible
    } __attribute__((packed));


    // MAC selection
    typedef IF<Traits<IEEE802_15_4_NIC>::tstp_mac, Raw_Header, Log_Header>::Result Header;
    typedef IF<Traits<IEEE802_15_4_NIC>::tstp_mac, Raw_Frame, Log_Frame>::Result Frame;
    typedef IF<Traits<TSTP>::enabled, TSTP_Metadata, Dummy_Metadata>::Result Metadata;
    typedef _UTIL::Buffer<NIC<IEEE802_15_4>, Frame, void, Metadata> Buffer;

    typedef _UTIL::Buffer<NIC<IEEE802_15_4>, Raw_Frame, void, TSTP_Metadata> TSTP_Buffer;
    typedef _UTIL::Buffer<NIC<IEEE802_15_4>, Log_Frame, void, Dummy_Metadata> ELP_Buffer;

    typedef Frame PDU;
    static const unsigned int MTU = Frame::MTU;

    typedef Data_Observer<Buffer, Protocol> Observer;
    typedef Data_Observed<Buffer, Protocol> Observed;

    // IEEE 802.15.4 NICs usually support SFD time stamping and the mostly do that with 64 bits
    // NICs that use different resolutions must have the time stamps adapted to 64 bits
    typedef unsigned long long Time_Stamp;

    // Configuration parameters
    struct Configuration: public NIC_Common::Configuration
    {
        friend Debug & operator<<(Debug & db, const Configuration & c) {
            db << "{unit=" << c.unit
               << ",addr=" << c.address
               << ",ch=" << c.channel
               << ",pw=" << c.power
               << ",p=" << c.period
               << ",o=" << c.offset
               << ",a=" << c.timer_accuracy
               << ",f=" << c.timer_frequency
               << "}";
            return db;
        }

        unsigned int unit;
        Address address;
        unsigned int channel;
        unsigned int power;
        Microsecond period;
        int offset;
        PPM timer_accuracy;
        Hertz timer_frequency;
    };

    // Meaningful statistics for IEEE 802.15.4
    struct Statistics: public NIC_Common::Statistics
    {
        Statistics(): tx_relayed(0), rx_overruns(0), tx_overruns(0), frame_errors(0), carrier_errors(0), collisions(0) {}

        friend Debug & operator<<(Debug & db, const Statistics & s) {
            db << "{rxp=" << s.rx_packets
               << ",rxb=" <<  s.rx_bytes
               << ",rxorun=" <<  s.rx_overruns
               << ",txp=" <<  s.tx_packets
               << ",txb=" <<  s.tx_bytes
               << ",txr=" <<  s.tx_relayed
               << ",txorun=" <<  s.tx_overruns
               << ",frm=" <<  s.frame_errors
               << ",car=" <<  s.carrier_errors
               << ",col=" <<  s.collisions
               << "}";
            return db;
        }

        Time_Stamp time_stamp;
        Count tx_relayed;
        Count rx_overruns;
        Count tx_overruns;
        Count frame_errors;
        Count carrier_errors;
        Count collisions;
    };

protected:
    IEEE802_15_4() {}

public:
    static const unsigned int mtu() { return Frame::MTU; }
    static const Address broadcast() { return Address::BROADCAST; }
};

__END_SYS

#endif

#endif

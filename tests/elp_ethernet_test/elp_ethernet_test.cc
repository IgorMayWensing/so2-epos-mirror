// EPOS Light Protocol Test Programs

#include <machine/nic.h>
#include <network/elp.h>
#include <time.h>
#include <communicator.h>

using namespace EPOS;

OStream cout;

static const unsigned int ITERATIONS = 10;

int elp_link_test(bool sender, NIC<ELP::NIC_Family> * nic, const NIC<ELP::NIC_Family>::Address & mac)
{
    cout << "\nLink<ELP> Test (";

    if(sender) { // sender
        cout << "Sender)" << endl;

        ELP::Address self(mac, 0);
        cout << "  Self:                       " << self << endl;

        NIC<ELP::NIC_Family>::Address peer_mac = mac;
        peer_mac[5]--;
        ELP::Address peer(peer_mac, 0);
        cout << "  Peer:                       " << peer << endl;

        Link<ELP> com(self, peer);
        char data[ELP::MTU];
        for(unsigned int i = 0; i < ITERATIONS; i++) {
            memset(data, '0' + i, ELP::MTU - 1);
            data[ELP::MTU - 1] = '\0';
            int sent = com.send(data, sizeof(data));
            if(sent == sizeof(data))
                cout << "  Data to " << peer << ": " << data << endl;
            else
                cout << "  Data was not correctly sent. It was " << sizeof(data) << " bytes long, but only " << sent << " bytes were sent!"<< endl;
        }
    } else { // receiver
        cout << "Receiver)" << endl;

        ELP::Address self(mac, 0);
        cout << "  Self:                       " << self << endl;

        NIC<ELP::NIC_Family>::Address peer_mac = mac;
        peer_mac[5]++;
        ELP::Address peer(peer_mac, 0);
        cout << "  Peer:                       " << peer << endl;

        Link<ELP> com(self, peer);
        char data[ELP::MTU];
        for(unsigned int i = 0; i < ITERATIONS; i++) {
            int received = com.receive(data, sizeof(data));
            if(received == sizeof(data))
                cout << "  Data from " << peer << ": " << data << endl;
            else
                cout << "  Data was not correctly received. It was " << sizeof(data) << " bytes long, but " << received << " bytes were received!"<< endl;
        }
    }

    ELP::NIC_Family::Statistics stat = nic->statistics();
    cout << "\n  Statistics:\n"
         << "  Tx Packets:                 " << stat.tx_packets << "\n"
         << "  Tx Bytes:                   " << stat.tx_bytes << "\n"
         << "  Rx Packets:                 " << stat.rx_packets << "\n"
         << "  Rx Bytes:                   " << stat.rx_bytes << endl;

    return stat.tx_bytes + stat.rx_bytes;
}

int elp_port_test(bool sender, NIC<ELP::NIC_Family> * nic, const NIC<ELP::NIC_Family>::Address & mac)
{
    cout << "\nPort<ELP> Test (";

    if(sender) { // sender
        cout << "Sender)" << endl;

        Delay(Second(1)); // Receiver takes more time to reach receiving state in the second execution.

        ELP::Address self(mac, 0);
        cout << "  Self:                       " << self << endl;

        NIC<ELP::NIC_Family>::Address peer_mac = mac;
        peer_mac[5]--;
        ELP::Address peer(peer_mac, 0);
        cout << "  Peer:                       " << peer << endl;

        Port<ELP> com(self);
        char data[ELP::MTU];
        for(unsigned int i = 0; i < ITERATIONS; i++) {
            memset(data, '0' + i, ELP::MTU - 1);
            data[ELP::MTU - 1] = '\0';
            int sent = com.send(peer, data, sizeof(data));
            if(sent == sizeof(data))
                cout << "  Data to " << peer << ": " << data << endl;
            else
                cout << "  Data was not correctly sent. It was " << sizeof(data) << " bytes long, but only " << sent << " bytes were sent!"<< endl;
        }
    } else { // receiver
        cout << "Receiver)" << endl;

        ELP::Address self(mac, 0);
        cout << "  Self:                       " << self << endl;

        NIC<ELP::NIC_Family>::Address peer_mac = mac;
        peer_mac[5]++;
        ELP::Address peer(peer_mac, 0);
        cout << "  Peer:                       " << peer << endl;

        Port<ELP> com(self);
        char data[ELP::MTU];

        for(unsigned int i = 0; i < ITERATIONS; i++) {
            int received = com.receive(&peer, data, sizeof(data));
            if(received == sizeof(data))
                cout << "  Data from " << peer << ": " << data << endl;
            else
                cout << "  Data was not correctly received. It was " << sizeof(data) << " bytes long, but " << received << " bytes were received!"<< endl;
        }
    }

    ELP::NIC_Family::Statistics stat = nic->statistics();
    cout << "\n  Statistics:\n"
        << "  Tx Packets:                 " << stat.tx_packets << "\n"
        << "  Tx Bytes:                   " << stat.tx_bytes << "\n"
        << "  Rx Packets:                 " << stat.rx_packets << "\n"
        << "  Rx Bytes:                   " << stat.rx_bytes << endl;

    return stat.tx_bytes + stat.rx_bytes;
}

int main()
{
    bool is_ethernet = EQUAL<ELP::NIC_Family, Ethernet>::Result;
    const char ethernet_name[] = "Ethernet";
    const char ieee802_15_4_name[] = "IEEE 802.15.4";
    const char * family = is_ethernet ? ethernet_name : ieee802_15_4_name;

    cout << "ELP Test\n" << endl;

    NIC<ELP::NIC_Family> * nic = Traits<ELP::NIC_Family>::DEVICES::Get<Traits<ELP>::NICS[0]>::Result::get(Traits<ELP>::NICS[0]);
    NIC<ELP::NIC_Family>::Address mac = nic->address();

    cout << "NIC:" << endl;
    cout << "  Family:                     " << family << endl;
    cout << "  Address:                    " << mac << endl;
    cout << "  MTU:                        " << ELP::NIC_Family::MTU << endl;
    cout << "  sizeof(NIC::Address):       " << sizeof(ELP::NIC_Family::Address) << endl;
    cout << "  sizeof(NIC::Frame):         " << sizeof(ELP::NIC_Family::Frame) << endl;

    cout << "\nELP:" << endl;
    cout << "  MTU                        " << ELP::MTU << endl;
    cout << "  sizeof(ELP::Address)       " << sizeof(ELP::Address) << endl;
    cout << "  sizeof(ELP::Header)        " << sizeof(ELP::Header) << endl;
    cout << "  sizeof(ELP::Packet)        " << sizeof(ELP::Packet) << endl;

    elp_link_test((mac[5] % 2), nic, mac);
    Delay(Second(5));
    elp_port_test((mac[5] % 2), nic, mac);
}

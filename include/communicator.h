// EPOS Communicator Declarations

#ifndef __communicator_h
#define __communicator_h

#include <network.h>
#include <synchronizer.h>

__BEGIN_SYS

// Commonalities for connectionless channels
template<typename Channel, bool connectionless>
class Communicator_Common: protected Channel::Observer, private Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition>
{
private:
    static const unsigned int HEADERS_SIZE = Channel::HEADERS_SIZE;

    typedef typename Channel::Observer::Observing_Condition Observing_Condition;
    typedef Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition> Observer;

public:
    // List to hold received Buffers
    typedef typename Channel::Buffer Buffer;
    typedef typename Buffer::List List;
    typedef typename List::Element Element;

    // Addresses
    typedef typename Channel::Address Address;

protected:
    Communicator_Common(const Address & address): _address(address) {
        Channel::attach(this, address);
    }

public:
    ~Communicator_Common() {
        Channel::detach(this, _address);
    }

    template<typename Message>
    int send(const Message & message) {
        return Channel::send(message);
    }
    int send(const Address & to, const void * data, unsigned int size) {
        return Channel::send(_address, to, data, size);
    }
    int send(const Address & from, const Address & to, const void * data, unsigned int size) {
        return Channel::send(from, to, data, size);
    }

    template<typename Message>
    int receive(const Message & message) {
        Buffer * buf = updated();
        return Channel::receive(buf, message);
    }
    int receive(void * data, unsigned int size) {
        Buffer * buf = updated();
        return Channel::receive(buf, data, size);
    }
    int receive(Address * from, void * data, unsigned int size) {
        Buffer * buf = updated();
        return Channel::receive(buf, from, data, size);
    }

    int receive_all(void * data, unsigned int size) { // block until "size" bytes are received
        int r = 0;
        for(unsigned int received = 0, coppied = 0; received < size; received += coppied) {
            Buffer * buf = updated();
            r += Channel::receive(buf, data + received, coppied = ((received + (buf->size() - HEADERS_SIZE)) > size ? (size - received) : (buf->size() - HEADERS_SIZE)));
        }
        return r;
    }
    int receive_all(Address * from, void * data, unsigned int size) { // block until "size" bytes are received
        int r = 0;
        for(unsigned int received = 0, coppied = 0; received < size; received += coppied) {
            Buffer * buf = updated();
            r += Channel::receive(buf, data + received, coppied = ((received + (buf->size() - HEADERS_SIZE)) > size ? (size - received) : (buf->size() - HEADERS_SIZE)));
        }
        return r;
    }

    template<typename Message>
    int reply(const Message & message) {
        return Channel::reply(message);
    }

private:
    void update(typename Channel::Observed * obs, const Observing_Condition & c, Buffer * buf) { Observer::update(c, buf); }
    Buffer * updated() { return Observer::updated(); }

protected:
    Address _address;
};

// Commonalities for connection-oriented channels
template<typename Channel>
class Communicator_Common<Channel, false>: protected Channel::Observer, private Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition>
{
private:
    static const unsigned int HEADERS_SIZE = Channel::HEADERS_SIZE;

    typedef typename Channel::Observer::Observing_Condition Observing_Condition;
    typedef Concurrent_Observer<typename Channel::Observer::Observed_Data, typename Channel::Observer::Observing_Condition> Observer;

public:
    // List to hold received Buffers
    typedef typename Channel::Buffer Buffer;
    typedef typename Buffer::List List;
    typedef typename List::Element Element;

    // Addresses
    typedef typename Channel::Address Address;

protected:
    Communicator_Common(const Address & address, const Address & peer): _address(address) {
        _connection = Channel::attach(this, address, peer);
    }

public:
    ~Communicator_Common() {
        Channel::detach(this, _connection);
    }

    int write(const void * data, unsigned int size) {
        return _connection->write(data, size);
    }

    int read_some(void * data, unsigned int size) { // receive up to "size" bytes from a single packet
        Buffer * buf = updated();
        return _connection->read(buf, data, size);
    }

    int read(void * d, unsigned int size) { // block until "size" bytes are received
        char * data = reinterpret_cast<char *>(d);
        unsigned int received = 0;
        do {
            Buffer * buf = updated();
            unsigned int segment_size = _connection->read(buf, data, size);
            data += segment_size;
            received += segment_size;
        } while(received <= size);
        return size;
    }

    int read_all(void * d, unsigned int size) { // block until "size" bytes are received considering only full segments
        char * data = reinterpret_cast<char *>(d);
        int r = 0;
        for(unsigned int received = 0, coppied = 0; received < size; received += coppied) {
            Buffer * buf = updated();
            r += _connection->read(buf, data + received, coppied = ((received + (buf->size() - HEADERS_SIZE)) > size ? (size - received) : (buf->size() - HEADERS_SIZE)));
        }
        return r;
    }

private:
    void update(typename Channel::Observed * obs, const Observing_Condition & c, Buffer * buf) { Observer::update(c, buf); }
    Buffer * updated() { return Observer::updated(); }

protected:
    Address _address;

    typename Channel::Connection * _connection;
};


// Link (point-to-point communicator) connectionless channels
template<typename Channel, bool connectionless>
class Link: private Communicator_Common<Channel, connectionless>
{
private:
    typedef Communicator_Common<Channel, connectionless> Base;

public:
    // Channel imports
    typedef typename Channel::Address Address;

public:
    Link(const Address & address, const Address & peer = Address::NULL): Base(address), _peer(peer) {}
    ~Link() {}

    int send(const void * data, unsigned int size) { return Base::send(Base::_address, _peer, data, size); }
    int receive(void * data, unsigned int size) { return Base::receive(data, size); }

    const Address & peer() const { return _peer;}

private:
    Address _peer;
};

// Link (point-to-point communicator) for connection-oriented channels
template<typename Channel>
class Link<Channel, false>: private Communicator_Common<Channel, false>
{
private:
    typedef Communicator_Common<Channel, false> Base;

public:
    // Channel imports
    typedef typename Channel::Address Address;

public:
    Link(const Address & address, const Address & peer = Address::NULL): Base(address, peer), _peer(peer) {}
    ~Link() {}

    int read(void * data, unsigned int size) { return Base::read_all(data, size); }
    int write(const void * data, unsigned int size) { return Base::write(data, size); }

    const Address & peer() const { return _peer;}

private:
    Address _peer;
};


// Port (1-to-N communicator) for connectionless channels
template<typename Channel, bool connectionless>
class Port: private Communicator_Common<Channel, connectionless>
{
private:
    typedef Communicator_Common<Channel, connectionless> Base;

public:
    // Channel imports
    typedef typename Channel::Address Address;

public:
    Port(const Address & address): Base(address) {}
    ~Port() {}

    template<typename Message>
    int send(const Message & message) { return Base::send(message); }
    int send(const Address & to, const void * data, unsigned int size) { return Base::send(to, data, size); }

    template<typename Message>
    int receive(const Message & message) { return Base::receive(message); }
    int receive(Address * from, void * data, unsigned int size) { return Base::receive(from, data, size); }

    template<typename Message>
    int reply(const Message & message) { return Base::reply(message); }
};

// Port (1-to-N communicator) for connection-oriented channels
template<typename Channel>
class Port<Channel, false>: private Communicator_Common<Channel, false>
{
private:
    typedef Communicator_Common<Channel, false> Base;

public:
    // Channel imports
    typedef typename Channel::Address Address;

public:
    Port(const Address & address): Base(address) {}
    ~Port() {}

    Link<Channel> * listen() { return new (SYSTEM) Link<Channel>(Channel::listen(this->_address)); }
    Link<Channel> * connect(const Address & to) { return new (SYSTEM) Link<Channel>(Channel::connect(this->_address, to)); }
};

__END_SYS

#endif

// EPOS Memory Declarations

#ifndef __memory_h
#define __memory_h

#include <architecture.h>

__BEGIN_SYS

class Address_Space: private MMU::Directory
{
    friend class Init_System;   // for Address_Space(pd)
    friend class Thread;        // for Address_Space(pd)
    friend class Scratchpad;    // for Address_Space(pd)
    friend class Task;          // for activate()

private:
    using MMU::Directory::activate;

public:
    typedef CPU::Phy_Addr Phy_Addr;
    typedef CPU::Log_Addr Log_Addr;

public:
    Address_Space();
    ~Address_Space();

    using MMU::Directory::pd;

    Log_Addr attach(Segment * seg);
    Log_Addr attach(Segment * seg, Log_Addr addr);
    void detach(Segment * seg);
    void detach(Segment * seg, Log_Addr addr);

    Phy_Addr physical(Log_Addr address);

private:
    Address_Space(MMU::Page_Directory * pd);
};


class Segment: public MMU::Chunk
{
    friend class Thread;        // for Segment(pt)
    friend class Init_End;        // for Segment(pt)

private:
    typedef MMU::Chunk Chunk;

public:
    typedef CPU::Phy_Addr Phy_Addr;
    typedef MMU::Flags Flags;

public:
    Segment(unsigned long bytes, Flags flags = Flags::APP);
    Segment(Phy_Addr phy_addr, unsigned long bytes, Flags flags);
    ~Segment();

    unsigned long size() const;
    Phy_Addr phy_address() const;
    long resize(long amount);

private:
    Segment(Phy_Addr pt, unsigned int from, unsigned int to, Flags flags): Chunk(pt, from, to, flags) {
        db<Segment>(TRC) << "Segment(pt=" << pt << ",from=" << from << ",to=" << to << ",flags=" << flags << ") [Chunk::pt=" << Chunk::pt() << ",sz=" << Chunk::size() << "] => " << this << endl;
    }
};

__END_SYS

#endif

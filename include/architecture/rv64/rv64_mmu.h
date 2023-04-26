// EPOS-- RISC-V 64 MMU Mediator Declarations

#ifndef __rv64_mmu_h
#define __rv64_mmu_h

#include <system/memory_map.h>
#include <utility/string.h>
#include <utility/list.h>
#include <utility/debug.h>
#include <architecture/cpu.h>
#include <architecture/mmu.h>

__BEGIN_SYS

class MMU : public MMU_Common<9, 9, 9, 12>
{
    friend class CPU;
    friend class Setup_SifiveU;

private:
    typedef Grouping_List<Frame> List;

    static const unsigned long PHY_MEM = Memory_Map::PHY_MEM;
    static const unsigned long RAM_BASE = Memory_Map::RAM_BASE;
    static const unsigned long APP_LOW = Memory_Map::APP_LOW;

public:
    // Page Flags
    class RV64_Flags
    {
    public:
        enum
        {
            V = 1 << 0,   // Valid
            R = 1 << 1,   // Readable
            W = 1 << 2,   // Writable
            X = 1 << 3,   // Executable
            U = 1 << 4,   // User
            G = 1 << 5,   // Global
            A = 1 << 6,   // Accesed
            D = 1 << 7,   // Dirty
            CT = 1 << 8,  // Contiguous
            MIO = 1 << 9, // I/O
            MASK = (1 << 10) - 1,
            APP = (V | R | W | X),
            SYS = (V | R | W | X | A | D),
            KC = (V | R | X),
            KD = (V | R | W),
            UD = (V | R | W | U),
            PD = (V | A | D),
            UA = (V | R | X | U)
        };

        RV64_Flags() {}
        RV64_Flags(const RV64_Flags &f) : _flags(f) {}
        RV64_Flags(unsigned int f) : _flags(f) {}
        RV64_Flags(const Flags &f) : _flags(((f & Flags::PRE) ? V : 0) |
                                            ((f & Flags::RW) ? (R | W) : R) |
                                            ((f & Flags::USR) ? U : 0) |
                                            ((f & Flags::EX) ? X : 0) | A | D) {}
        operator unsigned int() const { return _flags; }

    private:
        unsigned int _flags;
    };

    // Page_Table
    class Page_Table
    {
    private:
        PT_Entry ptes[PT_ENTRIES];

    public:
        Page_Table() {}

        PT_Entry &operator[](unsigned int i) { return ptes[i]; }

        PT_Entry get_entry(unsigned int i) { return ptes[i]; }

        // writes n=((from - to)/4096) PTEs to the page object 
        void map(RV64_Flags flags, int from, int to)
        {
            Phy_Addr *addr = alloc(to - from);
            if (addr)
            {
                remap(addr, flags, from, to);
            }
            else
            {
                for (; from < to; from++)
                {
                    ptes[from] = phy2pte(alloc(1), flags);
                }
            }
        }

        void remap(Phy_Addr addr, RV64_Flags flags, int from = 0, int to = 512, int size = sizeof(Page))
        {
            addr = align_page(addr);
            for (; from < to; from++)
            {
                Log_Addr *pte = phy2log(&ptes[from]);
                *pte = phy2pte(addr, flags);

                ptes[from] = phy2pte(addr, flags);
                addr += size;
            }
        }

        friend OStream & operator<<(OStream & os, Page_Table & pt) {
            os << "{\n";
            for(unsigned int i = 0; i < PT_ENTRIES; i++)
                if(pt[i]) {
                  os << "[" << i << "] \t" << pde2phy(pt[i]) << " " << hex << pde2flg(pt[i]) << dec << "\n";
                }
            os << "}";
            return os;
        }
    };

    void recursive_destruct(Phy_Addr addr) {
        for (unsigned int i = 0; i < PT_ENTRIES; i++) {
            if ((*addr)[i] & RV64_Flags::V) {
                if (!(RV64_Flags::X & (*addr)[i]) && !(RV64_Flags::R & (*addr)[i]) && !(RV64_Flags::W & (*addr)[i])) {
                    recursive_destruct(((pte2phy(*addr)[i])));
                }
            }
            (*addr)[i] = 0;
        }
    }

    // Chunk (for Segment)
    class Chunk
    {
    public:
        Chunk() {}

        // from 0 to bytes % PAGE_SIZE
        // pts = number of page_tables
        // pages = number of pages
        Chunk(Phy_Addr phy_addr, unsigned int bytes, Flags flags)
        {   
            _from = 0;
            _to = pages(bytes);
            _pt1s_total = page_tables(_to - _from);
            _pts = pages(bytes) + _pts;
            _bytes = bytes;
            _flags = RV64_Flags(flags);
            unsigned int allocated_bytes = 0;
            unsigned int addressed_bytes_l0 = Page::SIZE * PT_ENTRIES;

            for (unsigned int i = 0; i < _pts; i++) {
                Page_Table * _pt1 = phy2log(calloc(1));
                for (unsigned int j = 0; j < PT_ENTRIES; j++) {
                    Page_Table * _pt0 = phy2log(calloc(1));
                    if (allocated_bytes - _to > addressed_bytes_l0) {
                    _pt0->remap(phy_addr, _from, _to, flags);
                    } else {
                        _pt0->remap(phy_addr, _from, _to, flags);
                _       pt1_array[i] = phy2pte(_pt0, _flags);
                        break;
                    }
                    allocated_bytes += addressed_bytes_l0;
                _   pt1_array[i][j] = phy2pte(_pt0, _flags);
                }
            }            
        }
        Chunk(unsigned int bytes, Flags flags);
        Chunk(unsigned int bytes, Flags flags, EPOS::Color color)
        {   
            _from = 0;
            _to = pages(bytes);
            _pt1s_total = page_tables(_to - _from);
            _pts = pages(bytes) + _pts;
            _bytes = bytes;
            _flags = RV64_Flags(flags);
            unsigned int allocated_bytes = 0;
            unsigned int addressed_bytes_l0 = Page::SIZE * PT_ENTRIES;

            for (unsigned int i = 0; i < _pts; i++) {
                Page_Table * _pt1 = phy2log(calloc(1));
                for (unsigned int j = 0; j < PT_ENTRIES; j++) {
                    Page_Table * _pt0 = phy2log(calloc(1));
                    if (allocated_bytes - _to > addressed_bytes_l0) {
                    _pt0->map(_flags, 0, addressed_bytes_l0);
                    } else {
                        _pt0->map(_flags, 0, _to - allocated_bytes);
                _       pt1_array[i] = phy2pte(_pt0, _flags);
                        break;
                    }
                    allocated_bytes += addressed_bytes_l0;
                _   pt1_array[i] = phy2pte(_pt0, _flags);
                }
            }            
        }

        ~Chunk()
        {
            for(int i = 0; i < PT_ENTRIES; i++) {
                Page_Table* pt1 = _pt1_array[i];
                recursive_destruct(pt1);
            }
            // for (; _from < _to; _from++)
            //     free((*static_cast<Page_Table *>(phy2log(_pt)))[_from]);
            // free(_pt, _pts);
        }

        unsigned int pts() const { return _pts; }
        Page_Table *pt() const { return _pt; }
        unsigned int size() const { return _bytes; }
        RV64_Flags flags() const {return _flags;}
        int resize(unsigned int amount) { return 0; }
        Phy_Addr phy_address() const;

    private:
        unsigned int _from;
        unsigned int _to;
        // Total number of page tables
        unsigned int _pts;
        // Total number of l1 page tables
        unsigned int _pt1s_total;
        unsigned int _bytes;
        RV64_Flags _flags;
        Page_Table* _pt1_array[PT_ENTRIES];
};

    // Page Directory
    typedef Page_Table Page_Directory;

    // Directory (for Address_Space)
    class Directory
    {
    public:
        Directory() : _pd(phy2log(calloc(1))), _free(true)
        {
            // db<MMU>(WRN) << "Build Directory: " << endl;

            // for (unsigned int i = 0; i < PT_ENTRIES_LVL_1; i++)
            // {
            //     (*_pd)[i] = _master->get_entry(i);
            // }
        }

        Directory(Page_Directory *pd) : _pd(pd), _free(false) {}

        ~Directory()
        {
            recursive_destruct(_pd);
            // if (_free)
            //     free(_pd);
        }

        Page_Table *pd() const { return _pd; }

        void activate() const {
          db<MMU>(WRN) << "activate satp: " << endl;
          db<MMU>(WRN) << _pd << endl;

          CPU::satp((1UL << 63) | (reinterpret_cast<unsigned long>(_pd) >> 12));
          MMU::flush_tlb();
        }

        // Attach Chunk's PT into the Address Space and return the Page Directory base address.
        Log_Addr attach(const Chunk &chunk, unsigned int lvl2 = directory_lvl_2(APP_LOW))
        {
            // lvl2 will be the 9 bit index of the l2 page table
            // lvl2 is the line in l2 that'll index the l1 PT
            db<MMU>(WRN) << "Attach Iterativo: " << lvl2 << endl;
            for (unsigned int i = lvl2; i < PD_ENTRIES_LVL_2; i++)
                if (attach(i, chunk.pt(), chunk.pts(), chunk.flags()))
                {
                    Log_Addr addr = i << (DIRECTORY_SHIFT_LVL_2);
                    return addr;
                }
            return false;
        }

        // Used to create non-relocatable segments such as code
        Log_Addr attach(const Chunk &chunk, const Log_Addr &addr)
        {
            unsigned long lvl2 = directory_lvl_2(addr);
            db<MMU>(WRN) << "Attach lvl2: " << lvl2 << endl;

            if (!attach(lvl2, chunk.pt(), chunk.pts(), chunk.flags()))
                return Log_Addr(false);

            Log_Addr result = lvl2 << (DIRECTORY_SHIFT_LVL_2);
            return result;
        }

        void detach(const Chunk &chunk, unsigned int lvl2 = directory_lvl_2(APP_LOW))
        {
            for (unsigned int i = lvl2; i < PD_ENTRIES_LVL_2; i++)
            {
                if (_pd->get_entry(lvl2))
                {
                    detach(i, chunk.pt(), chunk.pts());
                    return;
                }
            }
            db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ") failed!" << endl;
        }

        void detach(const Chunk &chunk, Log_Addr addr)
        {
            unsigned int lvl2 = directory_lvl_2(addr);
            if (indexes(pte2phy(_pd->get_entry(lvl2))) != indexes(chunk.pt()))
            {
                db<MMU>(WRN) << "MMU::Directory::detach(pt=" << chunk.pt() << ",addr=" << addr << ") failed!" << endl;
                return;
            }
            detach(lvl2, chunk.pt(), chunk.pts());
        }

        Phy_Addr physical(Log_Addr addr) { return addr; }

    private:
        //   attach(l2 index of l1, ONE l1 address of the chunk, nº of l1s, flags)
        //   attach will link the l1 PTs into the l2 PT
        bool attach(unsigned int lvl2, const Page_Table *pt, unsigned int n, RV64_Flags flags)
        {
            if ((lvl2 > PD_ENTRIES_LVL_2 - 1) || _pd->get_entry(lvl2))
                return false;

            _pd->remap(pd1, RV64_Flags::V, lvl2, lvl2+1);
            return true
        }

        void detach(unsigned int lvl2, Page_Table *pt, unsigned int n)
        {
            for (unsigned int i = lvl2; i < lvl2 + n; i++)
            {
                (*pd1)[i] = 0;
            }
        }

    private:
        Page_Directory *_pd;
        bool _free;
    };

public:
    MMU() {}

    static Phy_Addr alloc(unsigned int frames = 1)
    {
        Phy_Addr phy(false);
        unsigned long size = 0;
        if (frames)
        {
            List::Element *e = _free.search_decrementing(frames);
            if (e)
            {
                db<MMU>(INF) << "Object: " << e->object() << endl;
                size = e->size();
                phy = e->object() + e->size(); // PAGE_SIZE
            }
            else
            {
                db<MMU>(ERR) << "MMU::alloc() failed!" << endl;
            }
            db<MMU>(INF) << "MMU::alloc(frames=" << frames << ") => " << phy << endl;
            db<MMU>(INF) << "MMU::List Element: " << sizeof(List::Element) << endl;
        }

        db<MMU>(INF) << "Size: " << size << endl;

        return phy;
    };

    static Phy_Addr calloc(unsigned int frames = 1)
    {
        Phy_Addr phy = alloc(frames);
        memset(phy2log(phy), 0, frames); //PAGE_SIZE
        return phy;
    }

    static void free(Phy_Addr addr, unsigned long n = 1)
    {
        db<MMU>(TRC) << "MMU::free(addr=" << addr << ",n=" << n << ")" << endl;

        addr = indexes(addr);

        assert(Traits<CPU>::unaligned_memory_access || !(addr % 4));

        if (addr && n)
        {
            List::Element *e = new (addr) List::Element(addr, n);
            List::Element *m1, *m2;
            db<MMU>(TRC) << "Creating Element: " << &m2 << endl;
            _free.insert_merging(e, &m1, &m2);
        }
        db<MMU>(TRC) << "List Size: " << _free.size() << endl;
    }

    static unsigned int allocable() { return _free.head() ? _free.head()->size() : 0; }

    static void master(Page_Directory * pd) {_master = pd;}

    static Page_Directory *volatile current() { return static_cast<Page_Directory *volatile>(phy2log(CPU::pdp())); }

    static void flush_tlb() { CPU::flush_tlb(); }
    static void flush_tlb(Log_Addr addr) { CPU::flush_tlb(addr); }

// private:
    static void init();

    // static Log_Addr phy2log(const Phy_Addr & phy) { return phy + (PHY_MEM - RAM_BASE); }
    static Log_Addr phy2log(const Phy_Addr &phy) { return phy; }

    static PD_Entry phy2pde(Phy_Addr bytes) { return ((bytes >> 12) << 10) | RV64_Flags::V; }

    static Phy_Addr pde2phy(PD_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }

    static PT_Entry phy2pte(Phy_Addr bytes, RV64_Flags flags) { return ((bytes >> 12) << 10) | flags; }

    static Phy_Addr pte2phy(PT_Entry entry) { return (entry & ~RV64_Flags::MASK) << 2; }

    static RV64_Flags pde2flg(PT_Entry entry) { return (entry & RV64_Flags::MASK); }

private:
    static List _free;
    static Page_Directory *_master;
};
__END_SYS

#endif

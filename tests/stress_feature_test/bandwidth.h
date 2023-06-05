#ifndef _BAND_
#define _BAND_

#define CACHE_LINE_SIZE 64       /* cache Line size is 64 byte */
#define DEFAULT_ALLOC_SIZE_KB 16384 //524288 //16384


/**************************************************************************
 * Public Types
 **************************************************************************/
enum access_type { READ, WRITE};

/**************************************************************************
 * Global Variables
 **************************************************************************/
const unsigned int G_MEM_SIZE = DEFAULT_ALLOC_SIZE_KB;// = DEFAULT_ALLOC_SIZE_KB * 1024;       /* memory size */
/*
unsigned long long  bench_read(unsigned int cpu)
{
    unsigned int i;
    unsigned long long  sum = 0;
    for ( i = 0; i < G_MEM_SIZE/sizeof(unsigned int); i+=(CACHE_LINE_SIZE/4) ) {
        sum += g_mem_ptr[cpu][i];
    }
    g_nread[cpu] += G_MEM_SIZE;
    return sum;
}
*/

unsigned int bench_write(unsigned int *g_mem_ptr)
{
    register unsigned int i;
    for ( i = 0; i < G_MEM_SIZE/sizeof(unsigned int); i+=(CACHE_LINE_SIZE/sizeof(unsigned int)) ) {
        g_mem_ptr[i] = i;
    }
    for ( i = 0; i < G_MEM_SIZE/sizeof(unsigned int); i+=(CACHE_LINE_SIZE/sizeof(unsigned int)) ) {
        g_mem_ptr[i] = i;
    }
    return G_MEM_SIZE;
}

inline int bandwidth(unsigned int *g_mem_ptr)
{
    unsigned long long  sum = 0;
    //unsigned int acc_type = 0;
    //if (read)
    //    acc_type = READ;
    //else
    //    acc_type = WRITE;

    //unsigned int i;

    // for (i = 0; i < G_MEM_SIZE / sizeof(unsigned int); i++) {
    //     g_mem_ptr[cpu][i] = i + cpu*i;
    // }

    //switch (acc_type) {
    //case READ:
    //    sum += bench_read(cpu);
    //    break;
    //case WRITE:
        sum += bench_write(g_mem_ptr);
    //    break;
    //}

    return (int) sum;
}
#endif
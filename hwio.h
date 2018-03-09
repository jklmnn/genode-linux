
#ifndef _HWIO_H_
#define _HWIO_H_

#define MMIO_SET_RANGE _IOW('g', 1, mmio_range_t*)
#define MMIO_SET_IRQ _IOW('g', 2, int*);

typedef struct {
    unsigned long phys;
    size_t length;
} mmio_range_t;

#endif //_HWIO_H_

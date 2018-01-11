
#ifndef _HWIO_H_
#define _HWIO_H_

#define MMIO_SET_RANGE _IOW('g', 1, mmio_range_t*)

typedef struct {
    unsigned long phys;
    size_t length;
} mmio_range_t;

#endif //_HWIO_H_

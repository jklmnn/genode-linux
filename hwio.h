
#ifndef _HWIO_H_
#define _HWIO_H_

#define MMIO_SET_RANGE _IOW('g', 1, mmio_range_t*)
#define IRQ_SET _IOW('g', 2, int*)

#define T_UNCONFIGURED 0
#define T_MMIO 1
#define T_IRQ 2

typedef int irq_t;

typedef struct {
    unsigned long phys;
    size_t length;
} mmio_range_t;

typedef struct {
    int type;
    union {
        mmio_range_t mmio;
        irq_t irq;
    };
} hwio_data_t;

#endif //_HWIO_H_

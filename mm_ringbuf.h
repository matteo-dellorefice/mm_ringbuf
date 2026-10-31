#ifndef MM_RINGBUF_DRIVER_H
#define MM_RINGBUF_DRIVER_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* ioctl defines */
#define MM_RINGBUF_IOC_MAGIC      'f'
#define MM_RINGBUF_SET_EVENTFD    _IOW(MM_RINGBUF_IOC_MAGIC, 1, int)

/* Ringbuf defines */
#define MM_RINGBUF_META_SIZE     (4 * 1024)
#define MM_RINGBUF_SLOT_SIZE     (1 * 1024 * 1024)
#define MM_RINGBUF_NUM_SLOTS     16
#define MM_RINGBUF_TOTAL_SIZE    (MM_RINGBUF_META_SIZE + (MM_RINGBUF_SLOT_SIZE * MM_RINGBUF_NUM_SLOTS))

struct mm_ringbuf_meta {
#ifdef __KERNEL__
    atomic_t head;
    atomic_t tail;
#else
    int32_t head;
    int32_t tail;
#endif
};

#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <stdint.h>

#include "mm_ringbuf.h"

int main() {
    int fd = open("/dev/mm_ringbuf0", O_RDWR);
    if (fd < 0) { perror("Open failed"); return 1; }

    int efd = eventfd(0, 0);
    if (efd < 0 || ioctl(fd, MM_RINGBUF_SET_EVENTFD, &efd) < 0) {
        perror("Eventfd/IOCTL setup failed");
        return 1;
    }

    void *map_base = mmap(NULL, MM_RINGBUF_TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) { perror("mmap failed"); return 1; }

    struct mm_ringbuf_meta *meta = (struct mm_ringbuf_meta *)map_base;
    printf("Reader running. Page-aligned slots begin at offset +4KB.\n");

    while (1) {
        uint64_t efd_val;
        if (read(efd, &efd_val, sizeof(efd_val)) < 0) break;

        // Atomic index evaluations
        int h = __atomic_load_n(&meta->head, __ATOMIC_ACQUIRE);
        int t = __atomic_load_n(&meta->tail, __ATOMIC_RELAXED);

        while (t != h) {
            // Perfect page boundary slot tracking
            size_t offset = MM_RINGBUF_META_SIZE + (t % MM_RINGBUF_NUM_SLOTS) * MM_RINGBUF_SLOT_SIZE;
            char *slot_ptr = (char *)map_base + offset;

            printf("Slot %d processed, first chars: %.10s\n", t, slot_ptr);

            t = (t + 1) % MM_RINGBUF_NUM_SLOTS;
            __atomic_store_n(&meta->tail, t, __ATOMIC_RELEASE);
        }
    }

    munmap(map_base, MM_RINGBUF_TOTAL_SIZE);
    close(efd);
    close(fd);
    return 0;
}
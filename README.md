# mm_ringbuf

A Linux kernel device driver stub that provides a event-signaled circular ring buffer interface between the Linux kernel and user space. 

`mm_ringbuf` exposes a memory mapped region organized as a ring buffer, and an asynchronous event notification mechanism (`eventfd`). User space processes can synch on the `eventfd` signal to consume data in the ringbuffer slots.

To simulate the production of data, the device file `/dev/mm_ringbuf0` can be written to with:

```bash
dd if=sample_payload.bin of=/dev/mm_ringbuf0 bs=1M count=1
```

Data will be copied to the next free slot in the ringbuffer. See `user_reader.c` for an example.

## Memory Layout

The driver allocates a memory region of `16MB + 4KB` using `vmalloc_user()`. 

The first 4KB is reserved for ring metadata (e.g. head and tail counters). The Following area is partitioned into 16 distinct 1MB slots.

```
+-------------------+-----------------+--------+     +---------+
| Metadata (4 KB)   | Slot 0 (1 MB)   | Slot 1 | ... | Slot 15 |
+-------------------+-----------------+--------+     +---------+
^                   ^                 ^
Base Mapping        Base + 4KB        Base + 4KB + 1MB
```

## Synchronization Pipeline

Synchronization is handled with an external `eventfd` instance generated in user space and passed to the module with a `ioctl` call. 


## Getting Started

### 1. Build & Installation
Compile both the kernel module and the sample test application by running the local Makefile configuration:

```bash
make
```

Load the module into your running environment:
```bash
make load
```

### 2. Configure Permissions (Udev Rule)
To grant non-root applications full read/write privileges to the shared framework interface automatically upon module insertion, add a matching udev rule:

```udev
# /etc/udev/rules.d/99-mm_ringbuf.rules
KERNEL=="mm_ringbuf0", SUBSYSTEM=="mm_ringbuf", MODE="0666"
```

**Reload settings with:** `sudo udevadm control --reload-rules && sudo udevadm trigger`

## License
This project is released under the GPL License.
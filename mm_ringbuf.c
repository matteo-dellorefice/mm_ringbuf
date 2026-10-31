#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/eventfd.h>

#include "mm_ringbuf.h"

#define DEVICE_NAME "mm_ringbuf0"
#define CLASS_NAME  "mm_ringbuf"

struct mm_ringbuf_dev {
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    void *ringbuf;
    struct eventfd_ctx *ev_ctx;
    struct mutex ev_lock;
};

static struct mm_ringbuf_dev device_ctx;

static int mm_ringbuf_open(struct inode *inode, struct file *filp) 
{ 
    return 0; 
}

static int mm_ringbuf_release(struct inode *inode, struct file *filp) 
{ 
    return 0; 
}

static int mm_ringbuf_mmap(struct file *filp, struct vm_area_struct *vma) 
{
    size_t size = vma->vm_end - vma->vm_start;

    if (size > MM_RINGBUF_TOTAL_SIZE)
        return -EINVAL;

    return remap_vmalloc_range(vma, device_ctx.ringbuf, 0);
}

static ssize_t mm_ringbuf_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) 
{
    struct mm_ringbuf_meta *meta = (struct mm_ringbuf_meta *)device_ctx.ringbuf;
    int head = atomic_read(&meta->head);
    int tail = atomic_read(&meta->tail);

    if (count > MM_RINGBUF_SLOT_SIZE)
        count = MM_RINGBUF_SLOT_SIZE;

    if (((head + 1) % MM_RINGBUF_NUM_SLOTS) == (tail % MM_RINGBUF_NUM_SLOTS))
        return -EBUSY; 

    size_t offset = MM_RINGBUF_META_SIZE + (head % MM_RINGBUF_NUM_SLOTS) * MM_RINGBUF_SLOT_SIZE;

    if (copy_from_user(device_ctx.ringbuf + offset, buf, count))
        return -EFAULT;

    smp_wmb(); 

    atomic_set(&meta->head, (head + 1) % MM_RINGBUF_NUM_SLOTS);
    
    dev_info(device_ctx.device, "Wrote %zu bytes into slot %d (new head=%d)\n", 
        count, head, (head + 1) % MM_RINGBUF_NUM_SLOTS);
    
    mutex_lock(&device_ctx.ev_lock);

    if (device_ctx.ev_ctx)
        eventfd_signal(device_ctx.ev_ctx);

    mutex_unlock(&device_ctx.ev_lock);

    return count;
}

static long mm_ringbuf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) 
{
    int efd;
    struct eventfd_ctx *ctx;
    struct mm_ringbuf_meta *meta = (struct mm_ringbuf_meta *)device_ctx.ringbuf;

    if (cmd != MM_RINGBUF_SET_EVENTFD) 
        return -ENOTTY;
    
    if (copy_from_user(&efd, (int __user *)arg, sizeof(int))) 
        return -EFAULT;

    ctx = eventfd_ctx_fdget(efd);

    if (IS_ERR(ctx)) {
        dev_err(device_ctx.device, "Failed to get eventfd context from fd %d\n", efd);
        return PTR_ERR(ctx);
    }

    mutex_lock(&device_ctx.ev_lock);

    if (device_ctx.ev_ctx) 
        eventfd_ctx_put(device_ctx.ev_ctx);

    device_ctx.ev_ctx = ctx;

    int h = atomic_read(&meta->head);
    int t = atomic_read(&meta->tail);
    
    if (h != t)
        eventfd_signal(device_ctx.ev_ctx);
    
    mutex_unlock(&device_ctx.ev_lock);

    dev_info(device_ctx.device, "Successfully registered eventfd handle %d\n", efd);
    return 0;
}

static const struct file_operations mm_ringbuf_fops = {
    .owner          = THIS_MODULE,
    .open           = mm_ringbuf_open,
    .release        = mm_ringbuf_release,
    .write          = mm_ringbuf_write,
    .mmap           = mm_ringbuf_mmap,
    .unlocked_ioctl = mm_ringbuf_ioctl,
};

static int __init mm_ringbuf_init(void) 
{
    dev_t dev;
    mutex_init(&device_ctx.ev_lock);

    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0) 
        return -1;

    device_ctx.major = MAJOR(dev);
    cdev_init(&device_ctx.cdev, &mm_ringbuf_fops);

    if (cdev_add(&device_ctx.cdev, dev, 1) < 0) 
        goto unreg_chrdev;

    device_ctx.class = class_create(CLASS_NAME);

    if (IS_ERR(device_ctx.class)) 
        goto del_cdev;

    device_ctx.device = device_create(device_ctx.class, NULL, dev, NULL, DEVICE_NAME);

    if (IS_ERR(device_ctx.device)) 
        goto destroy_class;

    device_ctx.ringbuf = vmalloc_user(MM_RINGBUF_TOTAL_SIZE);

    if (!device_ctx.ringbuf) 
        goto destroy_device;

    dev_info(device_ctx.device, "Stub driver loaded. Allocated %d bytes\n", MM_RINGBUF_TOTAL_SIZE);

    return 0;

destroy_device: device_destroy(device_ctx.class, dev);
destroy_class:  class_destroy(device_ctx.class);
del_cdev:       cdev_del(&device_ctx.cdev);
unreg_chrdev:   unregister_chrdev_region(dev, 1);
    return -ENOMEM;
}

static void __exit mm_ringbuf_exit(void) 
{
    dev_t dev = MKDEV(device_ctx.major, 0);

    if (device_ctx.ringbuf) 
        vfree(device_ctx.ringbuf);

    if (device_ctx.ev_ctx) 
        eventfd_ctx_put(device_ctx.ev_ctx);

    device_destroy(device_ctx.class, dev);
    class_destroy(device_ctx.class);
    cdev_del(&device_ctx.cdev);
    unregister_chrdev_region(dev, 1);

    pr_info("mm_ringbuf: module unloaded successfully\n");
}

module_init(mm_ringbuf_init);
module_exit(mm_ringbuf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matteo Dell'Orefice");
MODULE_DESCRIPTION("event-signaled circular ring buffer");

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/kfifo.h>

#define DEVICE_NAME "pchar"  // Device name for our char driver
#define FIFO_SIZE 1024      // FIFO size for buffer

// FIFO buffer
static DECLARE_KFIFO(my_fifo, char, FIFO_SIZE);

// Waiting queue for readers
static wait_queue_head_t rd_wq;

// Major number for the device
static int major_num;

// File operations for our device
static ssize_t pchar_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
static ssize_t pchar_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);
static int pchar_open(struct inode *inode, struct file *file);
static int pchar_release(struct inode *inode, struct file *file);

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = pchar_read,
    .write = pchar_write,
    .open = pchar_open,
    .release = pchar_release,
};

// Module initialization function
static int __init pchar_init(void)
{
    // Initialize the FIFO and waiting queue
    init_waitqueue_head(&rd_wq);

    // Register the character device
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) {
        printk(KERN_ALERT "pchar: Failed to register a major number\n");
        return major_num;
    }

    printk(KERN_INFO "pchar: Registered with major number %d\n", major_num);
    return 0;
}

// Module cleanup function
static void __exit pchar_exit(void)
{
    // Unregister the character device
    unregister_chrdev(major_num, DEVICE_NAME);
    printk(KERN_INFO "pchar: Unregistered the device\n");
}

// Open the device
static int pchar_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "pchar: Device opened\n");
    return 0;
}

// Release the device
static int pchar_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "pchar: Device closed\n");
    return 0;
}

// Read function: Block if the FIFO is empty, wake up when data is written
static ssize_t pchar_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    int ret;
    size_t bytes_read = 0;

    // Wait if the FIFO is empty
    while (kfifo_is_empty(&my_fifo)) {
        ret = wait_event_interruptible(rd_wq, !kfifo_is_empty(&my_fifo));
        if (ret) {
            printk(KERN_INFO "pchar: Read interrupted\n");
            return -ERESTARTSYS;  // Return error if the wait is interrupted
        }
    }

    // Read from the FIFO
    while (bytes_read < count && !kfifo_is_empty(&my_fifo)) {
        ret = kfifo_out(&my_fifo, &buf[bytes_read], 1);
        if (ret > 0) {
            bytes_read++;
        } else {
            break;  // Stop if no more data is available
        }
    }

    printk(KERN_INFO "pchar: Read %zu bytes\n", bytes_read);
    return bytes_read;
}

// Write function: Write to the FIFO and wake up the reader if it's blocked
static ssize_t pchar_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    int i;
    int ret;

    // Write data to the FIFO
    for (i = 0; i < count; i++) {
        ret = kfifo_in(&my_fifo, &buf[i], 1);
        if (ret != 1) {
            printk(KERN_ALERT "pchar: Failed to write to FIFO\n");
            return -ENOMEM;
        }
    }

    // Wake up the reader if it's waiting
    wake_up_interruptible(&rd_wq);

    printk(KERN_INFO "pchar: Written %zu bytes\n", count);
    return count;
}

module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shantanu ghadge <shantanughadge6@gmail.com>");
MODULE_DESCRIPTION("A simple char driver with blocking read and write synchronization.");


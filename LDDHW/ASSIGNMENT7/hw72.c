#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEVICE_NAME "pchar"  // Device name
#define BUF_SIZE 1024       // Buffer size

// Mutex for device access control
static DEFINE_MUTEX(dev_mutex);

// Device buffer
static char device_buffer[BUF_SIZE];

// Major number for the device
static int major_num;

// File operations
static int pchar_open(struct inode *inode, struct file *file);
static int pchar_release(struct inode *inode, struct file *file);
static ssize_t pchar_read(struct file *file, char __user *buf, size_t count, loff_t *pos);
static ssize_t pchar_write(struct file *file, const char __user *buf, size_t count, loff_t *pos);

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = pchar_open,
    .release = pchar_release,
    .read = pchar_read,
    .write = pchar_write,
};

// Module initialization function
static int __init pchar_init(void)
{
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

// Open the device: Ensure only one process can open it at a time
static int pchar_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&dev_mutex)) {
        // If mutex is already locked (device is open), block the process
        printk(KERN_INFO "pchar: Device is already open by another process. Blocking.\n");
        return -EBUSY;  // Device is busy, block this process
    }

    printk(KERN_INFO "pchar: Device opened\n");
    return 0;  // Device successfully opened
}

// Release the device: Unlock the mutex when the device is closed
static int pchar_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&dev_mutex);  // Release the mutex when the device is closed
    printk(KERN_INFO "pchar: Device closed\n");
    return 0;
}

// Read from the device: Simple echo functionality (example)
static ssize_t pchar_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    size_t bytes_read = count;

    if (*pos >= BUF_SIZE) {
        return 0;  // No more data to read
    }

    if (*pos + count > BUF_SIZE) {
        bytes_read = BUF_SIZE - *pos;  // Limit read size to available data
    }

    if (copy_to_user(buf, device_buffer + *pos, bytes_read)) {
        return -EFAULT;  // Error in copying data to user space
    }

    *pos += bytes_read;  // Update file position
    return bytes_read;
}

// Write to the device: Simple write functionality (example)
static ssize_t pchar_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    size_t bytes_written = count;

    if (*pos >= BUF_SIZE) {
        return -ENOSPC;  // No space left on device
    }

    if (*pos + count > BUF_SIZE) {
        bytes_written = BUF_SIZE - *pos;  // Limit write size to available space
    }

    if (copy_from_user(device_buffer + *pos, buf, bytes_written)) {
        return -EFAULT;  // Error in copying data from user space
    }

    *pos += bytes_written;  // Update file position
    return bytes_written;
}

module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("shantanu ghadge <shantanughadge6@gmail.com>");
MODULE_DESCRIPTION("A simple char driver with exclusive open access.");


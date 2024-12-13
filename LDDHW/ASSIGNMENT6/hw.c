#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "pseudo_char_device"
#define DEVICE_COUNT 2  // Number of device instances

// Define the ioctl commands
#define MY_IOCTL_CMD_RESIZE_FIFO _IOW('M', 1, size_t)

// FIFO resize function declaration
int fifo_resize(struct kfifo *fifo, size_t param);

// Device structure to store data related to each device
struct pseudo_device {
    struct cdev cdev;
    struct kfifo fifo;  // FIFO for each device instance
};

// Global variables
static int major_num;
static struct class *dev_class;
static struct pseudo_device *devices[DEVICE_COUNT];  // Array of devices

// Forward declarations for the functions
static int pseudo_open(struct inode *inode, struct file *filp);
static int pseudo_release(struct inode *inode, struct file *filp);
static long pseudo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

// File operations structure
static const struct file_operations pseudo_fops = {
    .owner = THIS_MODULE,
    .open = pseudo_open,
    .release = pseudo_release,
    .unlocked_ioctl = pseudo_ioctl,  // ioctl function
};

// IOCTL function to handle resizing FIFO
static long pseudo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct pseudo_device *dev = filp->private_data;
    int result = 0;

    switch (cmd) {
        case MY_IOCTL_CMD_RESIZE_FIFO:
            // Resize the FIFO using the parameter passed from user-space
            result = fifo_resize(&dev->fifo, (size_t)arg);
            if (result < 0) {
                pr_err("Failed to resize FIFO\n");
            }
            break;

        default:
            pr_err("Invalid ioctl command\n");
            return -ENOTTY;
    }

    return result;
}

// FIFO resize function implementation
int fifo_resize(struct kfifo *fifo, size_t param)
{
    void *temp_array;
    size_t fifo_size;

    // Step 1: Allocate a temporary array to store FIFO contents
    fifo_size = kfifo_size(fifo);
    temp_array = kmalloc(fifo_size, GFP_KERNEL);
    if (!temp_array) {
        pr_err("Failed to allocate memory for temp array\n");
        return -ENOMEM;
    }

    // Step 2: Copy FIFO contents into the temporary array
    if (kfifo_out(fifo, temp_array, fifo_size) != fifo_size) {
        pr_err("Failed to read all contents from FIFO\n");
        kfree(temp_array);
        return -EIO;
    }

    // Step 3: Release FIFO memory
    kfifo_free(fifo);

    // Step 4: Allocate new memory for the FIFO with the new size 'param'
    if (kfifo_alloc(fifo, param, GFP_KERNEL)) {
        pr_err("Failed to allocate new FIFO memory\n");
        kfree(temp_array);
        return -ENOMEM;
    }

    // Step 5: Copy contents from the temporary array into the new FIFO
    if (kfifo_in(fifo, temp_array, fifo_size) != fifo_size) {
        pr_err("Failed to write all contents back to FIFO\n");
        kfifo_free(fifo);
        kfree(temp_array);
        return -EIO;
    }

    // Step 6: Release the temporary array
    kfree(temp_array);

    pr_info("FIFO resized successfully to %zu bytes\n", param);

    return 0;
}

// Open function for the device
static int pseudo_open(struct inode *inode, struct file *filp)
{
    struct pseudo_device *dev;

    // Get the device structure from the inode
    dev = container_of(inode->i_cdev, struct pseudo_device, cdev);
    filp->private_data = dev;  // Store the device pointer in the file struct
    pr_info("Opened pseudo device\n");
    return 0;
}

// Release function for the device
static int pseudo_release(struct inode *inode, struct file *filp)
{
    struct pseudo_device *dev = filp->private_data;
    pr_info("Closed pseudo device\n");
    return 0;
}

// Initialize the device driver
static int __init pchar_init(void)
{
    int result;
    dev_t dev;

    // Allocate a major number dynamically
    result = alloc_chrdev_region(&dev, 0, DEVICE_COUNT, DEVICE_NAME);
    if (result < 0) {
        pr_err("Failed to allocate character device region\n");
        return result;
    }
    major_num = MAJOR(dev);

    // Create device class
    dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(dev_class)) {
        unregister_chrdev_region(dev, DEVICE_COUNT);
        pr_err("Failed to create device class\n");
        return PTR_ERR(dev_class);
    }

    // Initialize each device
    for (int i = 0; i < DEVICE_COUNT; i++) {
        devices[i] = kzalloc(sizeof(struct pseudo_device), GFP_KERNEL);
        if (!devices[i]) {
            pr_err("Failed to allocate memory for device %d\n", i);
            return -ENOMEM;
        }

        // Initialize the FIFO for each device
        if (kfifo_alloc(&devices[i]->fifo, 1024, GFP_KERNEL)) {
            pr_err("Failed to allocate FIFO for device %d\n", i);
            return -ENOMEM;
        }

        cdev_init(&devices[i]->cdev, &pseudo_fops);
        devices[i]->cdev.owner = THIS_MODULE;

        result = cdev_add(&devices[i]->cdev, MKDEV(major_num, i), 1);
        if (result) {
            pr_err("Failed to add cdev for device %d\n", i);
            return result;
        }

        // Create device file in /dev
        device_create(dev_class, NULL, MKDEV(major_num, i), NULL, "pseudo%d", i);
    }

    pr_info("Pseudo character device driver initialized\n");
    return 0;
}

// Cleanup the device driver
static void __exit pchar_exit(void)
{
    for (int i = 0; i < DEVICE_COUNT; i++) {
        device_destroy(dev_class, MKDEV(major_num, i));
        cdev_del(&devices[i]->cdev);
        kfifo_free(&devices[i]->fifo);
        kfree(devices[i]);
    }
    class_destroy(dev_class);
    unregister_chrdev_region(MKDEV(major_num, 0), DEVICE_COUNT);
    pr_info("Pseudo character device driver cleaned up\n");
}

// Module entry and exit points
module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux Device Driver with FIFO Resize and IOCTL support");
MODULE_AUTHOR("Shantanu Ghadge <shantanughadge6@.com>");


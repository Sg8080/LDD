#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kfifo.h>

#define DEVICE_NAME "pseudo_char_device"
#define DEVICE_COUNT 2  // Number of device instances

// FIFO resize function declaration (proto)
int fifo_resize(struct kfifo *fifo, size_t param);

// Define the module's init and exit functions
static int __init pchar_init(void)
{
    pr_info("Pseudo char driver initialized\n");
    return 0;
}

static void __exit pchar_exit(void)
{
    pr_info("Pseudo char driver exited\n");
}

// FIFO resize function
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

// Define module entry and exit points
module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Linux Device Driver with FIFO Resize");
MODULE_AUTHOR("Shantanu Ghadge <shantanughadge6@.com>");


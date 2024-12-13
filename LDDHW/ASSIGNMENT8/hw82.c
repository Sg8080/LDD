#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/timer.h>
#include <linux/sched.h>

#define DEVICE_NAME "pchar"
#define FIFO_SIZE 256

// Define ioctl commands
#define FIFO_START_TIMER _IO('p', 1)
#define FIFO_STOP_TIMER _IO('p', 2)

// FIFO buffer
static char mybuf[FIFO_SIZE];
static int fifo_head = 0;
static int fifo_tail = 0;

// Timer for periodic removal of characters
static struct timer_list fifo_timer;
static bool timer_running = false;

// Mutex for protecting FIFO access
static DEFINE_MUTEX(fifo_mutex);

// Function to log character and update FIFO
static void log_and_remove_char(struct work_struct *work)
{
    char c;
    mutex_lock(&fifo_mutex);
    if (fifo_head != fifo_tail) {
        c = mybuf[fifo_head];
        fifo_head = (fifo_head + 1) % FIFO_SIZE;
        pr_info("FIFO: removed character: '%c'\n", c);
    }
    mutex_unlock(&fifo_mutex);

    // If FIFO is empty, stop the timer
    if (fifo_head == fifo_tail) {
        pr_info("FIFO is empty. Stopping timer.\n");
        del_timer_sync(&fifo_timer);
        timer_running = false;
    }
}

// Timer callback function
static void fifo_timer_callback(struct timer_list *t)
{
    struct work_struct *work = (struct work_struct *)t;
    log_and_remove_char(work);
    // Restart timer if FIFO is not empty
    if (fifo_head != fifo_tail) {
        mod_timer(&fifo_timer, jiffies + msecs_to_jiffies(1000)); // 1 second delay
    }
}

// Open function for the character device
static int pchar_open(struct inode *inode, struct file *file)
{
    pr_info("pchar: device opened\n");
    return 0;
}

// Release function for the character device
static int pchar_release(struct inode *inode, struct file *file)
{
    pr_info("pchar: device closed\n");
    return 0;
}

// ioctl implementation
static long pchar_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
        case FIFO_START_TIMER:
            if (!timer_running) {
                pr_info("Starting the timer...\n");
                timer_setup(&fifo_timer, fifo_timer_callback, 0);
                mod_timer(&fifo_timer, jiffies + msecs_to_jiffies(1000)); // 1 second delay
                timer_running = true;
            } else {
                pr_info("Timer is already running.\n");
            }
            break;

        case FIFO_STOP_TIMER:
            if (timer_running) {
                pr_info("Stopping the timer...\n");
                del_timer_sync(&fifo_timer);
                timer_running = false;
            } else {
                pr_info("Timer is not running.\n");
            }
            break;

        default:
            return -EINVAL;  // Invalid command
    }

    return 0;
}

// File operations structure
static const struct file_operations pchar_fops = {
    .owner = THIS_MODULE,
    .open = pchar_open,
    .release = pchar_release,
    .unlocked_ioctl = pchar_ioctl,  // For ioctl calls
};

// Registering the device
static int __init pchar_init(void)
{
    int result;
    result = register_chrdev(0, DEVICE_NAME, &pchar_fops);  // Registering with dynamic major number
    if (result < 0) {
        pr_err("pchar: failed to register a device\n");
        return result;
    }
    pr_info("pchar: registered with major number %d\n", result);
    return 0;
}

static void __exit pchar_exit(void)
{
    unregister_chrdev(0, DEVICE_NAME);  // Unregister the device
    pr_info("pchar: unregistered the device\n");
}

module_init(pchar_init);
module_exit(pchar_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Character Driver with FIFO and Timer");
MODULE_AUTHOR("shantanu ghadge <shantanughadge6@gmail.com>");


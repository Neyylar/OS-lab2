

#include <linux/kernel.h>    
#include <linux/module.h>   
#include <linux/version.h>
#include <linux/fs.h>

#include <linux/if_tun.h>
#include <linux/if_macvlan.h>


#include <asm/uaccess.h>    
#include <linux/netdevice.h>
#include "chardev.h"

#define SUCCESS 0
#define DEVICE_NAME "char_dev"
#define BUF_LEN 100


static int Device_Open = 0;
static char Message[BUF_LEN];
static char output[BUF_LEN];
static char *Message_Ptr;


static struct memblock *res_memblock;

static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "device_open(%p)\n", file);

    if (Device_Open) {
        return -EBUSY;
    }

    Device_Open++;
    Message_Ptr = Message;
    try_module_get(THIS_MODULE);
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "device_release(%p,%p)\n", inode, file);
    Device_Open--;

    module_put(THIS_MODULE);
    return SUCCESS;
}

static ssize_t device_read(
        struct file *file,    
        char __user *buffer,    
        size_t length,    
        loff_t *offset
        ) {
    int bytes_read = 0;

    printk(KERN_INFO "device_read(%p,%p,%d)\n", file, buffer, length);
    if (*Message_Ptr == 0) {
        return 0;
    }

    while (length && *Message_Ptr) {
        put_user(*(Message_Ptr++), buffer++);
        length--;
        bytes_read++;
    }

    printk(KERN_INFO "Read %d bytes, %d left\n", bytes_read, length);
    return bytes_read;
}


static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
    int i;

    printk(KERN_INFO "device_write(%p,%s,%d)", file, buffer, length);

    for (i = 0;i < length && i < BUF_LEN; i++)
        get_user(Message[i], buffer + i);

    Message_Ptr = Message;
    return i;
}




#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int device_ioctl(struct inode *inode,    
                 struct file *file,   
                 unsigned int ioctl_num,   
                 unsigned long ioctl_param)
#else
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
#endif
{
    int i;
    char *temp;
    char ch;
    switch (ioctl_num) {
        case IOCTL_SET_MSG:
            temp = (char *) ioctl_param;
            get_user(ch, temp);
            for (i = 0; ch && i < BUF_LEN; i++, temp++) {
                get_user(ch, temp);
            }

            device_write(file, (char *) ioctl_param, i, 0);
            break;

        case IOCTL_GET_MSG:

            printk(KERN_INFO "Received msg from user_space: %s\n", Message);
            sprintf(output, "%s", "");
            if (strcmp(Message, "net_device") == 0) {
		struct net_device * n_dev;
		read_lock(&dev_base_lock);
            	n_dev = first_net_device(&init_net);
            	if (!n_dev) {
            	strcat(output, "No network devices ;(((");
            	}
		int c = 0;
		char buff_int[10];
		while (n_dev) {
			sprintf(buff_int, "%d", c);
			strcat(output, "Number of net device: ");
			strcat(output, buff_int);
			strcat(output, "\n");
			sprintf(buff_int, "%s", n_dev->name);
			strcat(output, buff_int);
			strcat(output, "\n");
			strcat(output, "State: ");
			sprintf(buff_int, "%d", n_dev->state);
			strcat(output, buff_int);
			n_dev = next_net_device(n_dev);
			c++;
			strcat(output, "\n\n");
		}
		strcat(output, "Total number of ");
		sprintf(buff_int, "%d", c);
		strcat(output, buff_int);
		read_unlock(&dev_base_lock);
		
            }
            else if (strcmp(Message, "pt_regs") == 0) {
                struct pt_regs *regs = task_pt_regs(current);
                char buff_int[10];
                strcat(output, "REGS: \n");
                strcat(output, "r10 ");
                sprintf(buff_int, "%d", regs->r10);
                strcat(output, buff_int);
		 strcat(output, "\nsp ");
                sprintf(buff_int, "%d", regs->sp);
                strcat(output, buff_int);

            }

            if (strcmp(output, "") == 0) {
                sprintf(output, "%s", "Wrong input");
            }
            strcat(output, "\n");

            printk(KERN_ALERT "output: %s", output);

            sprintf(Message, "%s", "");

            for (i = 0; i < strlen(output) && i < BUF_LEN; ++i) {
                Message[i] = output[i];
            }
            Message_Ptr = Message;

            i = device_read(file, (char *) ioctl_param, i, 0);

            put_user('\0', (char *) ioctl_param + i);
            break;

        case IOCTL_GET_NTH_BYTE:

            return Message[ioctl_param];
            break;
    }
    return SUCCESS;
}


struct file_operations Fops = {
        .owner   = THIS_MODULE,
        .read    = device_read,
        .write   = device_write,
        .open    = device_open,
        .release = device_release,    
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
        .ioctl = device_ioctl
#else
        .unlocked_ioctl = device_ioctl
#endif
};


int init_module() {
    int ret_val;
    ret_val = register_chrdev(MAJOR_NUM, DEVICE_NAME, &Fops);
    if (ret_val < 0) {
        printk(KERN_ALERT "%s failed with %d\n", "Sorry, registering the character device ", ret_val);
        return ret_val;
    }

    printk(KERN_INFO "\n%s The major device number is %d.\n", "Registration is a success", MAJOR_NUM);
    printk(KERN_INFO "If you want to talk to the device driver,\n");
    printk(KERN_INFO "you'll have to create a device file. \n");
    printk(KERN_INFO "We suggest you use:\n");
    printk(KERN_INFO "mknod %s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM);
    printk(KERN_INFO "The device file name is important, because\n");
    printk(KERN_INFO "the ioctl program assumes that's the\n");
    printk(KERN_INFO "file you'll use.\n");
	
    return 0;
}

void cleanup_module() {
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
    }
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tarasova Natalya");
MODULE_DESCRIPTION("OSI lab2");

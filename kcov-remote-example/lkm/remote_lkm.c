#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kcov.h>
#include <linux/workqueue.h>  
#include <linux/sched.h>
#include <linux/slab.h>

#define REMOTE_CMD (0xdead)

struct remote_obj {
	unsigned long handle;
	struct work_struct work;
	struct remote_arg* arg;
};

struct remote_arg {
	unsigned long size;
	unsigned char* buf;
};

void remote_worker(struct work_struct* work)
{
	struct remote_obj *obj = container_of(work, struct remote_obj, work);

	kcov_remote_start_common(obj->handle); // start coverage

	unsigned long arg_size = obj->arg->size;
	unsigned char* kbuf = kmalloc(arg_size, GFP_KERNEL);
	copy_from_user(kbuf, obj->arg->buf, arg_size);

	if (kbuf[0] == 'm' && kbuf[1] == 'l') {
		// null pointer dereference
		unsigned long ptr = 0x0;
		unsigned long ptr_val = *(unsigned long*)ptr;
	}

	kfree(kbuf);
	kfree(obj->arg);

	kcov_remote_stop(); 		// exit coverage
}

static int remote_open(struct inode *inode, struct file *file)
{
    pr_info("I have been awoken\n");
    struct remote_obj* obj = kmalloc(sizeof(struct remote_obj), GFP_KERNEL);
    if (!obj)
    	return -ENOMEM;
    obj->handle = kcov_common_handle();
    INIT_WORK(&(obj->work), remote_worker);
    file->private_data = obj;
    return 0;
}

static int remote_close(struct inode *inodep, struct file *filp)
{
    pr_info("Sleepy time\n");
    kfree(filp->private_data);
    return 0;
}

static ssize_t remote_write(struct file *file, const char __user *buf,
		       size_t len, loff_t *ppos)
{
    pr_info("Yummy - I just ate %d bytes\n", len);
    return len; /* But we don't actually do anything with the data */
}

static long remote_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct remote_arg* _arg = kmalloc(sizeof(struct remote_arg), GFP_KERNEL);
	struct remote_obj* obj = filp->private_data;

	pr_info("Cool, in ioctl now\n");

	if (copy_from_user(_arg, (void*)arg, sizeof(_arg))) 
		return -EINVAL;

	switch (cmd) {
		case REMOTE_CMD:
			obj->arg = _arg;
			schedule_work(&(obj->work));
			break;
		default:
			break;
	}
	return 0;
}

static const struct file_operations remote_fops = {
    .owner				= THIS_MODULE,
    .write				= remote_write,
    .open				= remote_open,
    .release			= remote_close,
    .llseek 			= no_llseek,
    .unlocked_ioctl 	= remote_ioctl,
};

struct miscdevice remote_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "remote_lkm",
    .fops = &remote_fops,
};

static int __init misc_init(void)
{
    int error;

    error = misc_register(&remote_device);
    if (error) {
        pr_err("can't misc_register :(\n");
        return error;
    }

    pr_info("I'm in\n");
    return 0;
}

static void __exit misc_exit(void)
{
    misc_deregister(&remote_device);
    pr_info("I'm out\n");
}

module_init(misc_init)
module_exit(misc_exit)

MODULE_DESCRIPTION("Simple Misc Driver");
MODULE_AUTHOR("f0rm2l1n");
MODULE_LICENSE("GPL");

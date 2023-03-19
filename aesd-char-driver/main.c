/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *Reference: 1) Linux Device Drive Edition Chapter 3
 * 	     2) https://github.com/cu-ecen-aeld/ldd3/blob/master/scull/main.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Rajesh Srirangam"); /** TODO: fill in the name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	return 0;
}
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t read_value = 0;
	ssize_t offset=0;			
	int available_bytes=0;  
	int bytes_read=0;
	 
	struct aesd_dev *dev = filp->private_data; 
	struct aesd_buffer_entry* buffer_entry = NULL;
	
	PDEBUG("Bytes read %zu with offset %lld",count,*f_pos);
	
	if (mutex_lock_interruptible(&dev->mutex1))			
	{
		return -ERESTARTSYS;   
	}

	buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &offset);
	if(buffer_entry == NULL)
	{
		goto exit;
	}
	
	available_bytes = buffer_entry->size - offset; 
	if(count < available_bytes)		
	{
		bytes_read=count;
	}
	else
	{ 
	   bytes_read = available_bytes;
	}
	
	if (copy_to_user(buf , (buffer_entry->buffptr + offset), bytes_read)) /*Storing content of kernel space to user space in buffer*/
	{
		read_value= -EFAULT;
		goto exit;
	}
	
	*f_pos += bytes_read;				/*Updating file pointer*/
	read_value = bytes_read;		
	exit:
		mutex_unlock(&dev->mutex1);
		return read_value;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t write_value ;
	size_t error_bytes_copy ;
	const char* ret = NULL;
	int status=0;
	struct aesd_dev* device = NULL;
	device = (filp->private_data);
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	
	status = mutex_lock_interruptible(&device->mutex1);
	if (status)              
	{	
		write_value= -ERESTARTSYS;				
		goto exit;
	}
	if (device->write_entry_value.size == 0)
	{
		device->write_entry_value.buffptr = kzalloc(count,GFP_KERNEL);
	}
	else
	{
		device->write_entry_value.buffptr = krealloc(device->write_entry_value.buffptr, device->write_entry_value.size + count, GFP_KERNEL);
	}
		
	if (device->write_entry_value.buffptr == NULL)
	{ 
		goto exit;
	}
        write_value=count;
	error_bytes_copy = copy_from_user((void *)(&device->write_entry_value.buffptr[device->write_entry_value.size]), buf, count);
	if(error_bytes_copy)
	{
		write_value = write_value - error_bytes_copy;		
	}
	
	device->write_entry_value.size = device->write_entry_value.size + (count - error_bytes_copy);
	if (strchr((char *)(device->write_entry_value.buffptr), '\n')) 
	{
		ret= aesd_circular_buffer_add_entry(&device->buffer, &device->write_entry_value);
        if(ret)
	   {
	      kfree(ret);
	   }
	 
        device->write_entry_value.buffptr = 0;
		device->write_entry_value.size = 0;
	}

    exit:
  	mutex_unlock(&device->mutex1);
	return write_value;	
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);
	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) 
	{
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) 
	{
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));
	aesd_circular_buffer_init(&aesd_device.buffer);
	mutex_init(&aesd_device.mutex1);
	result = aesd_setup_cdev(&aesd_device);

	if( result )
	{
		unregister_chrdev_region(dev, 1);
	}
	return result;
}

void aesd_cleanup_module(void)
{
        dev_t devno = 0;
	PDEBUG("Clean and Exit");
	devno = MKDEV(aesd_major, aesd_minor);
	cdev_del(&aesd_device.cdev);
	aesd_circular_buffer_exit(&aesd_device.buffer);
	mutex_destroy(&aesd_device.mutex1);
	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);


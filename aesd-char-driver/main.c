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

MODULE_AUTHOR("Rajesh"); /** TODO: fill in name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	struct aesd_dev *dev;
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
	ssize_t value = 0;
	ssize_t offset=0;			
	int bytes_available=0;  
	int bytes_read=0;
	int mutex_status=0;
	 
	struct aesd_dev *dev = filp->private_data; 
	struct aesd_buffer_entry* buffer_entry = NULL;
	
	PDEBUG("Bytes read %zu with offset %lld",count,*f_pos); 
	mutex_status = mutex_lock_interruptible(&dev->mutex1);
	if (mutex_status)			/*Return 0 if mutex obtained*/
	{
		return -ERESTARTSYS;   /*Kernel can re-execute when there is some interruption*/
	}
	else if(mutex_status == 0)
	{
		PDEBUG("Mutex Acquired"); 
	}	
	buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &offset);
	if(buffer_entry == NULL)
	{
		goto exit;
	}
	bytes_available = buffer_entry->size - offset; /*Calulating the total bytes available which can be read*/
	bytes_read=bytes_available;		/*Complete read */
	if(count < bytes_available)		/* Partial read */
	{
		bytes_read=count;
	}
	if (copy_to_user(buf , (buffer_entry->buffptr + offset), bytes_read)) /*Storing content of kernel space to user space in buffer*/
	{
		value = -EFAULT;
		goto exit;
	}
	
	*f_pos += bytes_read;				/*Updating file pointer*/
	value = bytes_read;		
	exit:
			mutex_unlock(&dev->mutex1);
			return value;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t value ;
	size_t bytes_not_copied ;
	const char* removed_entry = NULL;
	int mutex_status=0;
	
	struct aesd_dev* dev = NULL;
	dev = (filp->private_data);
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	
	mutex_status = mutex_lock_interruptible(&dev->mutex1);
	if (mutex_status)              /*Return 0 if mutex obtained*/
	{	
		value= -ERESTARTSYS;				/*Kernel can re-execute when there is some interruption*/
		goto exit;
	}
	else if(mutex_status == 0)
	{
		PDEBUG("Mutex Acquired"); 
	}
	/*If the input size is zero, alloc or use realloc if size is gretater than zero*/
	if (dev->write_entry_value.size == 0)
	{
		dev->write_entry_value.buffptr = kzalloc(count,GFP_KERNEL);
	}
	else
	{
		dev->write_entry_value.buffptr = krealloc(dev->write_entry_value.buffptr, dev->write_entry_value.size + count, GFP_KERNEL);
	}
		
	/*kalloc error condition*/
	if (dev->write_entry_value.buffptr == NULL)
	{ 
		goto exit;
	}

	bytes_not_copied = copy_from_user((void *)(&dev->write_entry_value.buffptr[dev->write_entry_value.size]), buf, count); /*Storing content to kernel space from user space buffer*/
	
	
	value=count;					/*Return value for Complete write */
	if(bytes_not_copied)
	{
		value = value - bytes_not_copied;		/*Return value for partial write*/
	}
	dev->write_entry_value.size = dev->write_entry_value.size + (count - bytes_not_copied);
	
	/*Using strchr, check if new line is present*/
	if (strchr((char *)(dev->write_entry_value.buffptr), '\n')) 
	{

		removed_entry = aesd_circular_buffer_add_entry(&dev->buffer, &dev->write_entry_value);
       	kfree(removed_entry);
        	dev->write_entry_value.buffptr = 0;
		dev->write_entry_value.size = 0;
	}

  exit:
  	mutex_unlock(&dev->mutex1);
	return value;

	
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

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	PDEBUG("Clean and Exit");
	dev_t devno = MKDEV(aesd_major, aesd_minor);
	cdev_del(&aesd_device.cdev);
	aesd_circular_buffer_exit(&aesd_device.buffer);
	mutex_destroy(&aesd_device.mutex1);
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);


#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/kdev_t.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/list.h>
#include<linux/mutex.h>
#include<linux/errno.h>
#include<linux/workqueue.h>

#define MAX_WORDS 1024
#define MAX_WORD_BYTES 32

dev_t dev = 0;
static struct class *dev_class;
static struct cdev module_cdev;
static bool module_opened = false;

struct word_node {
	struct list_head list;
	char *word;
};
static int words_count = 0;

static LIST_HEAD(words);
static DEFINE_MUTEX(words_mutex);

static struct workqueue_struct *word_logger_workqueue;
static struct delayed_work word_logger_task;

static char *read_buffer = NULL;

static int module_open(struct inode *inode, struct file *file);
static int module_release(struct inode *inode, struct file *file);
static ssize_t module_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t module_write(struct file *filp, const char *buf, size_t len, loff_t *off);

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = module_read,
	.write = module_write,
	.open = module_open,
	.release = module_release,
};

/**
 * read_buffer_generator - word list string generator
 *
 * Returns the bytes of the generated string.
 *
 * Generates a string with space separated words from the word list.
 */
static size_t read_buffer_generator(void)
{
	char *position = NULL;
	struct word_node *word_first, *tmp;
	size_t space_remaining = MAX_WORDS * MAX_WORD_BYTES;

	if (read_buffer) {
		kfree(read_buffer);
		read_buffer = NULL;
	}

	if (list_empty(&words)) {
		read_buffer = kmalloc(1, GFP_KERNEL);
		if (!read_buffer) {
			pr_alert("Memory allocation failed\n");
			kfree(read_buffer);
			read_buffer = NULL;
			return -ENOMEM;
		}
		read_buffer[0] = '\0';
		return 1;
	}

	read_buffer = kmalloc(MAX_WORDS * MAX_WORD_BYTES + 1, GFP_KERNEL);
	if (!read_buffer) {
		pr_alert("Memory allocation failed\n");
		kfree(read_buffer);
		read_buffer = NULL;
		return -ENOMEM;
	}

	position = read_buffer;
	mutex_lock(&words_mutex);
	list_for_each_entry_safe(word_first, tmp, &words, list) {
		size_t word_length = strlen(word_first->word);
		if (word_length+1 <= space_remaining) {
			if (position != read_buffer) {
				*position++ = ' ';
				space_remaining--;
			}
			strncpy(position, word_first->word, word_length);
			position += word_length;
			space_remaining -= word_length;
		} else {
			break;
		}
	}
	mutex_unlock(&words_mutex);
	*position++ = '\0';

	return (position - read_buffer);
}

/**
 * word_logger - logging workqueue function 
 *
 * Logs the first word in the word list every second and then removes it.
 */
static void word_logger(struct work_struct *work)
{
	if (!list_empty(&words)) {
		struct word_node *word_first;
		mutex_lock(&words_mutex);
		word_first = list_first_entry(&words, struct word_node, list);
		list_del(&word_first->list);
		mutex_unlock(&words_mutex);
		words_count--;

		pr_info("Word: %s\n", word_first->word);

		kfree(word_first->word);
		word_first->word = NULL;
		kfree(word_first);
		word_first = NULL;
	}

	queue_delayed_work(word_logger_workqueue, &word_logger_task, HZ);
}

static int module_open(struct inode *inode, struct file *file)
{
	if (module_opened)
		return -EBUSY;
	module_opened = true;

	pr_info("Device file opened\n");
	return 0;
}

static int module_release(struct inode *inode, struct file *file)
{
	module_opened = false;

	pr_info("Device file closed\n");
	return 0;
}

/**
 * module_read - text transfer to userspace 
 *
 * Copy the words from internal memory to userspace.
 */
static ssize_t module_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t copy_bytes = 0;

	copy_bytes = read_buffer_generator();

	if (len < copy_bytes)
		copy_bytes = len;

	if (copy_to_user(buf, read_buffer, copy_bytes)) {
		pr_warn("Read Error\n");
		kfree(read_buffer);
		read_buffer = NULL;
		return -EFAULT;
	}

	return copy_bytes;
}

/**
 * module_read - text transfer from userspace 
 *
 * Copy the string from userspace to internal memory.
 * Skips too many words or words that are too long.
 */
static ssize_t module_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	char *tmp_buf;
	char *token;

	if (len <= 0) {
		pr_warn("Write request empty\n");
		return len;
	}

	tmp_buf = kmalloc(len + 1, GFP_KERNEL);
	if (!tmp_buf) {
		pr_alert("Memory allocation failed\n");
		return -ENOMEM;
	}

	if (copy_from_user(tmp_buf,buf,len)) {
		pr_warn("Write Error\n");
		kfree(tmp_buf);
		tmp_buf = NULL;
		return -EFAULT;
	}

	tmp_buf[len] = '\0';

	while ((token = strsep(&tmp_buf, " ")) != NULL) {
		if (strlen(token)+1 > MAX_WORD_BYTES) {
			pr_warn("Word length reached\n");
		} else if (strlen(token) > 0) {
			if (words_count < MAX_WORDS) {
				struct word_node *new_word;
				new_word = kmalloc(sizeof(struct word_node), GFP_KERNEL);
				if (!new_word) {
					pr_alert("Memory allocation failed\n");
					kfree(tmp_buf);
					tmp_buf = NULL;
					return -ENOMEM;
				}

				new_word->word = kstrdup(token, GFP_KERNEL);
				if (!new_word->word) {
					pr_alert("Memory allocation failed\n");
					kfree(new_word);
					new_word = NULL;
					kfree(tmp_buf);
					tmp_buf = NULL;
					return -ENOMEM;
				}

				mutex_lock(&words_mutex);
				list_add_tail(&new_word->list, &words);
				mutex_unlock(&words_mutex);
				words_count++;
			} else {
				pr_warn("Word limit reached\n");
				break;
			}
		}
	}

	kfree(tmp_buf);
	tmp_buf = NULL;
	return len;
}

int init_module(void)
{
	if ((alloc_chrdev_region(&dev, 0, 1, "module_driver")) < 0) {
		pr_warn("Cannot allocate the major number\n");
		return -1;
	}
	pr_info("Major = %d Minor = %d\n",MAJOR(dev), MINOR(dev));

	cdev_init(&module_cdev, &fops);
	if ((cdev_add(&module_cdev, dev, 1)) < 0) {
		pr_warn("Cannot add the device to the system\n");
		goto r_class;
	}

	if ((dev_class = class_create(THIS_MODULE, "module_class")) == NULL) {
		pr_warn("Cannot create the struct class\n");
		goto r_class;
	}

	if ((device_create(dev_class, NULL, dev, NULL, "module_device")) == NULL) {
		pr_warn("Cannot create the device\n");
		goto r_device;
	}

	word_logger_workqueue = create_workqueue("word_logger_workqueue");
	if (!word_logger_workqueue) {
		pr_warn("Cannot create the workqueue\n");
		return -ENOMEM;
	}
	INIT_DELAYED_WORK(&word_logger_task, word_logger);
	queue_delayed_work(word_logger_workqueue, &word_logger_task, HZ);

	pr_info("Module loaded\n");
	return 0;

r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	return -1;
}

void cleanup_module(void)
{
	struct word_node *word_first, *tmp;
	list_for_each_entry_safe(word_first, tmp, &words, list) {
		list_del(&word_first->list);
		kfree(word_first->word);
		word_first->word = NULL;
		kfree(word_first);
		word_first = NULL;
	}

	cancel_delayed_work_sync(&word_logger_task);
	flush_workqueue(word_logger_workqueue);
	destroy_workqueue(word_logger_workqueue);

	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	cdev_del(&module_cdev);
	unregister_chrdev_region(dev, 1);

	pr_info("Module unloaded\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Felix Reineke");
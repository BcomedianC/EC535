#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system_misc.h> /* cli(), *_flags */
#include <linux/uaccess.h>
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/jiffies.h> /* get access to jiffies variable */
#include <linux/timer.h> /* kernel timer utilities */
#include <linux/string.h> /* string functions */
//#include <fs/proc/internal.h> //struct proc

#define MAX_TIMER 5
#define BUFFERSIZE 140 * MAX_TIMER

MODULE_LICENSE("GPL");
MODULE_LICENSE("Timer Kernel Module");
MODULE_AUTHOR("Buyuan Lin");

// Function declarations
static int 		mytimer_fasync(int fd, struct file *filp, int mode);
static int 		mytimer_open(struct inode *inode, struct file *filp);
static int 		mytimer_release(struct inode *inode, struct file *filp);
static ssize_t  mytimer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t  mytimer_write(struct file *filp, const char *buf, size_t count, loff_t *fpos);
static int 		mytimer_init(void);
static void 	mytimer_exit(void);
void mytimer_callback(struct timer_list *data);
static int mytimer_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);


static const struct file_operations mytimer_fops = 
{
	fasync:   mytimer_fasync,
	open:     mytimer_open,
	release:  mytimer_release,
	read:     mytimer_read,
	write:    mytimer_write
};

//struct of timer, with message
struct T_Timer
{
	struct timer_list this_timer;
	char user_message[128];
};

struct T_Timer *timer_Array; //Declare timer array for storage
static char user_Flag[2] = {""}; //store flag like -l, -s
static int mytimer_count = 0; //number of timers running
static char mytimer_update[156]; //reset message string
static char *updated = mytimer_update; //reset string pointer

//Declaration of module init and exit functions
module_init(mytimer_init);
module_exit(mytimer_exit);

//borrowed from cookie stuff
static unsigned capacity = 128;
static unsigned bite = 128;
struct fasync_struct *async_queue;

static int mytimer_major = 61;
module_param(mytimer_major, uint, S_IRUGO);//signal handling
static struct proc_dir_entry *mytimer_proc;
int time_loaded;

static char *message_buffer; //user message buffer
static int message_len; //user message length

// Initialize fasync struct
static int mytimer_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &async_queue);
}

//Open with success
static int mytimer_open(struct inode *inode, struct file *filp)
{
	return 0;
}

//Release with fasync successfully
static int mytimer_release(struct inode *inode, struct file *filp)
{
	mytimer_fasync(-1, filp, 0);
	return 0;
}

//Timer read
static ssize_t mytimer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int temp;
	unsigned int time_left;
	char temp_str[256];
	char *temp_str_ptr = temp_str;
	char final_str[256] = {""};
	char *final_str_ptr = final_str;
    //int i;
	//int counter = 0;

	*f_pos = -1;

	//String pointer reaches the end of the message
	if (*f_pos >= message_len) return 0;

	//Stop at the end
	if (count > message_len - *f_pos)
		count = message_len - *f_pos;

	if (copy_from_user(message_buffer + *f_pos, buf, count))
	{
		return -EFAULT;
	}

	//update temp_str_ptr for different parameters
	temp_str_ptr += sprintf(
		temp_str_ptr, 
		"read called: process id %d, command %s, count %d, chars ",
		current -> pid, current -> comm, count);

	//read from message_buffer
	for (temp = *f_pos; temp < count + *f_pos; temp++)
	{
		temp_str_ptr += sprintf(temp_str_ptr, "%c", message_buffer[temp]);
	}

	*f_pos += count + 1;

	//-l flag
	if(!strcmp(user_Flag, "-l"))
	{
		//print all timers
		if(mytimer_count == 0)
		{
			if(strcmp(timer_Array[0].user_message, ""))
			{
				time_left = jiffies_to_msecs((timer_Array[0].this_timer.expires - jiffies))/1000;
				final_str_ptr += sprintf(final_str_ptr, "%s", timer_Array[0].user_message);
				final_str_ptr += sprintf(final_str_ptr, "%u\n", time_left);
			}
			count = strlen(final_str);
			if(copy_to_user(buf, final_str, strlen(final_str)))
			{
				return -EFAULT;
			}

			return count;
		}
	}

	else if(!strcmp(user_Flag, "-s"))
	{
		count = strlen(mytimer_update);
		if(copy_to_user(buf, updated, strlen(mytimer_update)))
		{
			return -EFAULT;
		}
		memset(updated, 0, sizeof(mytimer_update));
		return count;
	}
		return count;
}

static ssize_t mytimer_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	int ret;
	int temp;
	unsigned long time = 0;
	char *end_ptr;
	char temp_str[256];
	char *temp_str_ptr = temp_str;
	char *update_str;
	unsigned bla = 156;
	int timer_Index = 0;
	char *check_Timer;
	bool check = true;
    int i;

	if (*f_pos >= capacity)
	{
		printk(KERN_INFO
			"write called: process id %d, command %s, count %d, buffer full\n",
			current->pid, current->comm, count);
		return -ENOSPC;
	}

	//cookie
	if (count > bite) count = bite;

	//stop before the end
	if (count > capacity - *f_pos)
		count = capacity - *f_pos;

	//copy from user
	if (copy_from_user(message_buffer + *f_pos, buf, count))
	{
		return -EFAULT;
	}

	temp_str_ptr += sprintf(temp_str_ptr,								   
		"write called: process id %d, command %s, count %d, chars ",
		current->pid, current->comm, count);

	for (temp = *f_pos; temp < count + *f_pos; temp++)					  
		temp_str_ptr += sprintf(temp_str_ptr, "%c", message_buffer[temp]);

	*f_pos += count;
	message_len = *f_pos;

	check_Timer = kmalloc(count, GFP_KERNEL);
	if(!check_Timer)
		printk("Error with checkTimer kmalloc");

	update_str = kmalloc(bla, GFP_KERNEL);
	if(!update_str)
		printk("Error with updateStr kmalloc");

	//get flag
	for(i = 0; i < 2; i++)
	{
		user_Flag[i] = buf[i];
	}

	if(!strcmp(user_Flag, "-s"))
	{
		sprintf(update_str, "%s", buf+(3));//"-s plus space"
		//same idea I used for HW2, however strtok could not bu used here
		for(i = 0; i < count - 3; i++)
		{
			if(update_str[i] == ' ')
			{
				time = simple_strtoul(update_str, &end_ptr, 10); //get the time number
				strcpy(check_Timer, update_str + (i + 1)); //get the name of timer
				break;
			}
		}
		time = time * 1000; //micro second conversion

		//check if timer already exists or not
		for(i = 0; i < mytimer_count; i++)
		{
			if(!strcmp(timer_Array[i].user_message, check_Timer))
			{
				timer_Index = i; //store the existing timer index in the array
				check = false;
				break;
			}
		}

		//if doesn't exist, put in message
		if(check)
		{
			for(i = 0; i < mytimer_count; i++)
			{
				if(!strcmp(timer_Array[i].user_message, ""))
				{
					timer_Index = i;
					break;
				}
			}
			strcpy(timer_Array[timer_Index].user_message, check_Timer);
		}

		//set or update timer
		ret = mod_timer(&timer_Array[timer_Index].this_timer, jiffies + msecs_to_jiffies(time));

		//If already exist, display update info
		if(ret)
		{
			memset(updated, 0, sizeof(*updated));
			sprintf(updated, "Timer %s has now been reset to %lu seconds!", timer_Array[timer_Index].user_message, time);
		}
	}
	//for -s flag
	else if(!strcmp(user_Flag, "-s"))
	{
		memset(update_str, 0, sizeof(*update_str));
		sprintf(update_str, "%s", buf+(3));
		mytimer_count = simple_strtoul(update_str, &end_ptr, 10);
	}

	return count;
}

static int mytimer_init(void)
{
	int result;
	int i;
	time_loaded = jiffies;

	//Register device
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	//Exception for invilid major numer
	if (result < 0)
	{
		printk(KERN_INFO "mytimer: Invalid major number %d\n", mytimer_major);
		return result;
	}

	//Exception for memory allocation 
	timer_Array = kmalloc(sizeof(struct T_Timer) * MAX_TIMER, GFP_KERNEL);
	if(!timer_Array)
	{
		printk(KERN_ALERT "mytimer: Insufficient kernel memory\n");
		result = -ENOMEM;
		goto fail;
	}
	memset(timer_Array, 0, sizeof(struct T_Timer) * MAX_TIMER);

	mytimer_count = 1;

	//Setup linux timer
	for(i = 0; i < mytimer_count; i++)
	{
		timer_setup(&timer_Array[i].this_timer, mytimer_callback, i);
		timer_Array[i].user_message[0] = '\0';
	}

	message_buffer = kmalloc(BUFFERSIZE, GFP_KERNEL);
	if(!message_buffer)
	{
		printk(KERN_ALERT "mytimer: Insufficient kernel memory\n");
		result = -ENOMEM;
		goto fail;
	}

	memset(message_buffer, 0, BUFFERSIZE);

	mytimer_proc = proc_create("mytimer", 0, NULL, &mytimer_fops);
	if(!mytimer_proc)
		return -ENOMEM;
	else
	{
		//mytimer_proc->read_proc = mytimer_proc_read;
	}

	printk(KERN_ALERT "Inserting mytimer module\n");
	return 0;

fail:
    mytimer_exit();
    return result;
}

static void mytimer_exit(void)
{
	// Freeing the major number 
	unregister_chrdev(mytimer_major, "mytimer");
	// Removing proc entrys
	remove_proc_entry("mytimer", NULL);

	// Freeing buffer memory 
	if (message_buffer)
	{
		kfree(message_buffer);
	}
	
	printk(KERN_ALERT "Removing mytimer module\n");
}


void mytimer_callback(struct timer_list *mytimer)
{
	if (async_queue)
		kill_fasync(&async_queue, SIGIO, POLL_IN);

	del_timer(mytimer);
	mytimer_count = 0;
}

static int mytimer_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/* get expiration time and time since module load */
	int time_since_load = jiffies_to_msecs(jiffies-time_loaded)/1000;
	int time_left = jiffies_to_msecs(timer_Array->this_timer.expires-jiffies)/1000;

	/* output information to proc entry page */
	if(mytimer_count == 1)
		count = sprintf(page, "Module name: mytimer\nTime since loaded (sec): %d\nProcess ID of user program: %d\nCommand name: ktimer\nTime until expiration (sec): %d\n", time_since_load, current->pid, time_left);
	else
		count = sprintf(page, "Module name: mytimer\nTime since loaded (sec): %d\n", time_since_load);
	return count;
}


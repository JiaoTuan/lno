//'Hello world' netfilter hooks example
//For any packet, we drop it, and log fact to /var/log/messages

#undef _KERNEL_

#include <linux/kernel.h> //required for any kernel modules
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#include <linux/module.h> 	//This is a kernel module.
#include <linux/kernel.h> 	//This is a kernel module.
#include <linux/proc_fs.h> 	//Since we are using proc file system.
#include<linux/sched.h>		//For scheduling.
#include <asm/uaccess.h>	//copy_to_user(), copy_from_user().
#include <linux/slab.h>		//for kmalloc() and kfree()

#include <linux/skbuff.h>
#include <linux/ip.h>         // for IP header

#include <linux/string.h>

#define _KERNEL_

//initialize to unblock
bool block = false;

bool incoming = false;

bool outgoing = false;

//for incoming traffic
static struct nf_hook_ops nfhoIN; //struct holding set of hook function options

//for outgoing traffic
static struct nf_hook_ops nfhoOUT;

//two proc files, one for incoming, and the other one for outgoing.

struct sk_buff *sock_buff;
struct iphdr *ip_header;

char *msg1;
int len1,temp1;

char *msg2;
int len2,temp2;

ssize_t write_proc1(struct file *filp,const char *buf,size_t count,loff_t *offp)
{

	//copy the data from the buffer to msg (in the proc entry).
	copy_from_user(msg1,buf,count);
	strim(msg1);

	//Update len and temp for msg.
	len1=count;
	temp1=len1;

	// redundant?
	if (count > 0) {
	   incoming = !incoming;
	}

	//***check to make sure the character buffers are comparable

	//return the number of bytes copied.
	return count;
}

struct file_operations proc_fops1 = {

	//Those are both callback functions
	write: write_proc1,
};

void create_new_proc_entry(void)
{

	//Here we pass the file_operations struct toe create the proc entry.
	//NULL: means it's proc/hello
	proc_create("blockAll",0666,NULL,&proc_fops0);

	//GFP_KERNEL: most reliable, will sleep or swap if out of memory.
	//here we need to allocate more space to entry the message.
	msg0=kmalloc(100*sizeof(char),GFP_KERNEL);

	//For entering incoming address to block
	proc_create("incoming",0666,NULL,&proc_fops1);

	//GFP_KERNEL: most reliable, will sleep or swap if out of memory.
	msg1=kmalloc(100*sizeof(char),GFP_KERNEL);

	//For entering outgoing address to block
	proc_create("outgoing",0666,NULL,&proc_fops2);

	//GFP_KERNEL: most reliable, will sleep or swap if out of memory.
	msg2=kmalloc(100*sizeof(char),GFP_KERNEL);
}

//This hook function is registered on NF_IP_LOCAL_IN
unsigned int hook_funcIN(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {

	if(block){
		printk(KERN_INFO "incoming packet dropped\n");
		return NF_DROP;
	}else if(incoming){
	    unsigned int src_ip;
	    char source[16];

	    unsigned int dest_ip;
	    char dest[16];

	    sock_buff = skb;
	    ip_header = (struct iphdr *) skb_network_header(sock_buff);
	    // get source and dest IP
	    src_ip = (unsigned int) ip_header -> saddr;
	    dest_ip = (unsigned int) ip_header -> daddr;

	    int slen = snprintf(source,16,"%p4",&src_ip);
	    int dlen = snprintf(dest, 16, "%pI4", &dest_ip);

	    if(strstr(msg1,source) != NULL){
        	printk(KERN_INFO "incoming packet from source addr: %s dropped", source);
        	return NF_DROP;

        }else{
        	printk(KERN_INFO "incoming packet from source addr: %s accepted", source);
        	printk(KERN_INFO "incoming packet to dest addr: %s accepted", dest);

        	return NF_ACCEPT;
        }
	}

}

//Called when module loaded using "insmod"
int init_module(){

	//setup nf_hook_ops:
	//struct for incoming traffic
	nfhoIN.hook = hook_funcIN; //function to call when conditions below met
	nfhoIN.hooknum = NF_INET_LOCAL_IN;
	nfhoIN.pf = PF_INET;  //IPV4 packets. (ignore IPV6)
	nfhoIN.priority = NF_IP_PRI_FIRST; //set to highest priority over all other hook functions
	nf_register_hook(&nfhoIN); //register hook

	//struct for outgoing traffic
	nfhoOUT.hook = hook_funcOUT;
	nfhoOUT.hooknum = NF_INET_LOCAL_OUT;
	nfhoOUT.pf = PF_INET; //IPV4 packets. (ignore IPV6)
	nfhoOUT.priority = NF_IP_PRI_FIRST;
	nf_register_hook(&nfhoOUT); //register hook


	//Create two proc_entries: one for blockAll and the other one for monitoring.
	create_new_proc_entry();

	return 0;
}
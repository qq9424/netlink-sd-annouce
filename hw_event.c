/*
 * Copyright 2007-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * mxc_hw_event.c
 * Collect the hardware events, send to user by netlink
 */

#define SD_ONLINE  "sdOn "
#define SD_OFFLINE  "sdOff"
#include <linux/kthread.h>




#include <linux/init.h>  
#include <linux/module.h>  
#include <linux/kernel.h>  
#include <linux/skbuff.h>  
#include <linux/ip.h>  
#include <linux/types.h>  
#include <linux/sched.h>  
#include <linux/netlink.h>  
#include <net/sock.h>  

#define USER_NETLINK_CMD    25  
#define MAXMSGLEN           1024  
#define HW_EVENT_GROUP		2
  
typedef enum error_e {  
    NET_ERROR,  
    NET_OK,  
    NET_PARAM,  
    NET_MEM,  
    NET_SOCK,  
} netlink_err;  
  
typedef enum module_e {  
    HELLO_CMD = 1,  
} netlink_module;  
  
typedef enum type_e {  
    HELLO_SET,  
    HELLO_GET,  
} netlink_type;  

static struct task_struct *hwevent_kthread;
  
MODULE_LICENSE("Dual BSD/GPL");  
MODULE_AUTHOR("MDAXIA");  
  
struct sock *netlink_fd;  

 DECLARE_WAIT_QUEUE_HEAD(sdevent_wq);

 int sd_status = 0, sd_changed = 0;

EXPORT_SYMBOL( sd_status );
EXPORT_SYMBOL( sd_changed );
EXPORT_SYMBOL( sdevent_wq);  

void netlinkToUser()
{
	struct nlmsghdr *nl;  
	struct sk_buff *skb;  
	int size;  
	int len =7;
	
	size = NLMSG_SPACE(len);  
	skb = alloc_skb(size, GFP_ATOMIC);	
	if(!skb )  
	{  
		printk(KERN_ALERT "netlink_to_user skb of buf null!\n");  
		return;  
	}  
	nl = nlmsg_put(skb, 0, 0, 0, NLMSG_SPACE(len) - sizeof(struct nlmsghdr), 0);  
	NETLINK_CB(skb).pid = 0;  
	NETLINK_CB(skb).dst_group = 0;	

	if( sd_status )
		memcpy(NLMSG_DATA(nl), SD_ONLINE, len);  
	else
		memcpy(NLMSG_DATA(nl), SD_OFFLINE, len); 
	nl->nlmsg_len = (len > 2) ? (len - 2):len;	
  
	//netlink_unicast(netlink_fd, skb, dest, MSG_DONTWAIT);  
	netlink_broadcast(netlink_fd, skb, 0, HW_EVENT_GROUP, GFP_KERNEL);
	//printk( "K send:%d\n",sd_status); 

}

static int hw_event_thread(void *data)
{
	static int pre_sd_status = 0xffff ;

	LIST_HEAD(tmp_head);
	DEFINE_WAIT(wait);
	
	printk("lhg hw_event_thread start\n");
	
	while (1) {
		nlmsg_failure:
		msleep(200);

		wait_event (sdevent_wq, sd_changed );

		if( pre_sd_status != sd_status)
		{

			netlinkToUser();
			pre_sd_status = sd_status;
			sd_changed = 0;
		}
		
	}
	

	return 0;
}

  
static int __init netlink_init(void)  
{  
    netlink_fd = netlink_kernel_create(&init_net, NETLINK_USERSOCK, 0, NULL, NULL, THIS_MODULE);  
    if(NULL == netlink_fd)  
    {  
        printk(KERN_ALERT "Init netlink!\n");  
        return -1;  
    }  
	hwevent_kthread = kthread_run(hw_event_thread, NULL, "hwevent");
	if (IS_ERR(hwevent_kthread)) {
		printk(KERN_WARNING
		       "mxc_hw_event: Fail to create hwevent thread.\n");
		return 1;
	}
    printk(KERN_ALERT "Init netlink success!\n");  
    return 0;  
}  
  
static void __exit netlink_exit(void)  
{  
    netlink_kernel_release(netlink_fd);  
    printk(KERN_ALERT "Exit netlink!\n");  
}  
  
module_init(netlink_init);  
module_exit(netlink_exit); 


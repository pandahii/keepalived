/*
 * Soft:        Vrrpd is an implementation of VRRPv2 as specified in rfc2338.
 *              VRRP is a protocol which elect a master server on a LAN. If the
 *              master fails, a backup server takes over.
 *              The original implementation has been made by jerome etienne.
 *
 * Part:        vrrp.c program include file.
 *
 * Version:     $Id: vrrp.h,v 0.6.1 2002/06/13 15:12:26 acassen Exp $
 *
 * Author:      Alexandre Cassen, <acassen@linux-vs.org>
 *
 *              This program is distributed in the hope that it will be useful, 
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 */

#ifndef _VRRP_H
#define _VRRP_H

/* system include */
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

/* local include */
#include "vrrp_ipaddress.h"
#include "vrrp_ipsecah.h"
#include "vrrp_if.h"
#include "timer.h"
#include "utils.h"
#include "vector.h"

typedef struct {	/* rfc2338.5.1 */
	uint8_t		vers_type;	/* 0-3=type, 4-7=version */
	uint8_t		vrid;		/* virtual router id */
	uint8_t		priority;	/* router priority */
	uint8_t		naddr;		/* address counter */
	uint8_t		auth_type;	/* authentification type */
	uint8_t		adver_int;	/* advertissement interval(in sec) */
	uint16_t	chksum;		/* checksum (ip-like one) */
/* here <naddr> ip addresses */
/* here authentification infos */
} vrrp_pkt;

/* protocol constants */
#define INADDR_VRRP_GROUP 0xe0000012	/* multicast addr - rfc2338.5.2.2 */
#define VRRP_IP_TTL	255	/* in and out pkt ttl -- rfc2338.5.2.3 */
#define IPPROTO_VRRP	112	/* IP protocol number -- rfc2338.5.2.4*/
#define VRRP_VERSION	2	/* current version -- rfc2338.5.3.1 */
#define VRRP_PKT_ADVERT	1	/* packet type -- rfc2338.5.3.2 */
#define VRRP_PRIO_OWNER	255	/* priority of the ip owner -- rfc2338.5.3.4 */
#define VRRP_PRIO_DFL	100	/* default priority -- rfc2338.5.3.4 */
#define VRRP_PRIO_STOP	0	/* priority to stop -- rfc2338.5.3.4 */
#define VRRP_AUTH_NONE	0	/* no authentification -- rfc2338.5.3.6 */
#define VRRP_AUTH_PASS	1	/* password authentification -- rfc2338.5.3.6 */
#define VRRP_AUTH_AH	2	/* AH(IPSec) authentification - rfc2338.5.3.6 */
#define VRRP_ADVER_DFL	1	/* advert. interval (in sec) -- rfc2338.5.3.7 */
#define VRRP_PREEMPT_DFL 1	/* rfc2338.6.1.2.Preempt_Mode */


/*
 * parameters per vrrp sync group. A vrrp_sync_group is a set
 * of VRRP instances that need to be state sync together.
 */
typedef struct _vrrp_sgroup {
        char    *gname;         /* Group name */
        vector  iname;          /* Set of VRRP instances in this group */
        int     state;          /* current stable state */
} vrrp_sgroup;


/* parameters per virtual router -- rfc2338.6.1.2 */
typedef struct {
	uint32_t	addr;	/* the ip address */
	uint8_t		mask;	/* the ip address CIDR netmask */
	int		set;	/* TRUE if addr is set */
} vip_addr;

typedef struct _vrrp_rt {
	char	*iname;		/* Instance Name */
	vrrp_sgroup *sync;
	interface *ifp;		/* Interface we belong to */
	uint32_t  mcast_saddr;	/* Src IP address to use in VRRP IP header */
	char	*lvs_syncd_if;	/* handle LVS sync daemon state using this
                                 * instance FSM & running on specific interface
                                 * => eth0 for example.
                                 */
	int	vrid;		/* virtual id. from 1(!) to 255 */
	int	priority;	/* priority value */
	int	naddr;		/* number of ip addresses */
	int	vipset;		/* All the vips are set ? */
	vip_addr *vaddr;	/* point on the ip address array */
	int	neaddr;		/* number of excluded ip addresses */
	vip_addr *evaddr;	/* list of protocol excluded VIPs.
	                         * Those VIPs will not be presents into the
                                 * VRRP adverts
                                 */
	int	adver_int;	/* delay between advertisements(in sec) */	
	char	hwaddr[6];	/* VMAC -- rfc2338.7.3 */
	int	preempt;	/* true if a higher prio preempt a lower one */
	int	state;		/* internal state (init/backup/master) */
	int	init_state;	/* the initial state of the instance */
	int	wantstate;	/* user explicitly wants a state (back/mast) */
	int	fd;		/* the socket descriptor */

	int     debug;          /* Debug level 0-4 */

	/* State transition notification */
	int	smtp_alert;
	int     notify_exec;
	char    *script_backup;
	char    *script_master;
	char    *script_fault;

	/* rfc2336.6.2 */
	uint32_t	ms_down_timer;
	struct timeval	sands;

	/* Authentication data */
	int		auth_type;	/* authentification type. VRRP_AUTH_* */
	uint8_t		auth_data[8];	/* authentification data */

	/*
	 * To have my own ip_id creates collision with kernel ip->id
	 * but it should be ok because the packets are unlikely to be
	 * fragmented (they are non routable and small)
	 * This packet isnt routed, i can check the outgoing MTU
	 * to warn the user only if the outoing mtu is too small
	 */
	int		ip_id;

	/* IPSEC AH counter def --rfc2402.3.3.2 */
	seq_counter *ipsecah_counter;
} vrrp_rt;

/* VRRP state machine -- rfc2338.6.4 */
#define VRRP_STATE_INIT			0	/* rfc2338.6.4.1 */
#define VRRP_STATE_BACK			1	/* rfc2338.6.4.2 */
#define VRRP_STATE_MAST			2	/* rfc2338.6.4.3 */
#define VRRP_STATE_FAULT		3	/* internal */
#define VRRP_STATE_GOTO_MASTER		4	/* internal */
#define VRRP_STATE_DUMMY_MAST		5	/* internal */
#define VRRP_STATE_LEAVE_MASTER		6	/* internal */
#define VRRP_STATE_GOTO_DUMMY_MAST	97	/* internal */
#define VRRP_STATE_GOTO_FAULT		98	/* internal */
#define VRRP_DISPATCHER 		99	/* internal */
#define VRRP_MCAST_RETRY		10	/* internal */
#define VRRP_MAX_FSM_STATE		5	/* internal */

/* VRRP packet handling */
#define VRRP_PACKET_OK       0
#define VRRP_PACKET_KO       1
#define VRRP_PACKET_DROP     2
#define VRRP_PACKET_NULL     3
#define VRRP_PACKET_OTHER    4      /* Muliple VRRP on LAN, Identify "other" VRRP */

/* VRRP Packet fixed lenght */
#define VRRP_MAX_VIP		20
#define VRRP_PACKET_TEMP_LEN	512
#define VRRP_AUTH_LEN		8
#define VRRP_VIP_TYPE		(1 << 0)
#define VRRP_EVIP_TYPE		(1 << 1)

#define VRRP_IS_BAD_VID(id)		((id)<1 || (id)>255)	/* rfc2338.6.1.vrid */
#define VRRP_IS_BAD_PRIORITY(p)		((p)<1 || (p)>255)	/* rfc2338.6.1.prio */
#define VRRP_IS_BAD_ADVERT_INT(d) 	((d)<1)
#define VRRP_IS_BAD_DEBUG_INT(d)	((d)<0 || (d)>4)

#define VRRP_TIMER_SKEW(srv)	((256-(srv)->priority)*TIMER_HZ/256) 
#define VRRP_VIP_ISSET(V)	((V)->vipset)

#define VRRP_MIN(a, b)	((a) < (b)?(a):(b))
#define VRRP_MAX(a, b)	((a) > (b)?(a):(b))

#define VRRP_PKT_SADDR(V) (((V)->mcast_saddr)?(V)->mcast_saddr:IF_ADDR((V)->ifp))

/* prototypes */
extern int open_vrrp_socket(const int proto, const int index);
extern void new_vrrp_socket(vrrp_rt *vrrp);
extern void close_vrrp_socket(vrrp_rt *vrrp);
extern void vrrp_send_gratuitous_arp(vrrp_rt *vrrp);
extern int vrrp_send_adv(vrrp_rt *vrrp, int prio);
extern int vrrp_state_fault_rx(vrrp_rt *vrrp, char *buf, int buflen);
extern int vrrp_state_master_rx(vrrp_rt *vrrp, char *buf, int buflen);
extern void vrrp_state_master_tx(vrrp_rt *vrrp, const int prio);
extern void vrrp_state_backup(vrrp_rt *vrrp, char *buf, int buflen);
extern void vrrp_state_goto_master(vrrp_rt *vrrp);
extern void vrrp_state_leave_master(vrrp_rt *vrrp);
extern int vrrp_ipsecah_len(void);
extern void vrrp_complete_init(void);
extern void shutdown_vrrp_instances(void);

#endif

/*	$OpenBSD: if_san_common.h,v 1.8 2005/11/08 20:23:42 canacar Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__IF_SAN_COMMON_H
#define	__IF_SAN_COMMON_H

# include <dev/pci/if_san_te1.h>
# include <dev/pci/if_sandrv.h>

#define ADDR_MASK(x,y) (((caddr_t)(x) - (caddr_t)0) & (y))

#define WANPIPE_LITE_VERSION	"1.1.1"
#define WAN_OPENBSD_PLATFORM	0x06
#define WAN_PLATFORM_ID	WAN_OPENBSD_PLATFORM
#define	WANPIPE_MAGIC	0x414C4453L	/* signature: 'SDLA' reversed */

#define	ROUTER_NAME	"wanrouter"	/* in case we ever change it */
#define	ROUTER_IOCTL	'W'		/* for IOCTL calls */

#define WANROUTER_MAJOR_VER	2
#define WANROUTER_MINOR_VER	1

/* IOCTL codes for /proc/router/<device> entries (up to 255) */
#define WANPIPE_DUMP	_IOW(ROUTER_IOCTL, 16, wan_conf_t)
#define WANPIPE_EXEC	_IOWR(ROUTER_IOCTL, 17, wan_conf_t)

/* get monitor statistics */
#define SIOC_WANPIPE_PIPEMON	_IOWR('i', 150, struct ifreq)

/* set generic device */
#define SIOC_WANPIPE_DEVICE	_IOWR('i', 151, struct ifreq)

/* get hwprobe string */
#define SIOC_WANPIPE_HWPROBE	_IOWR('i', 152, struct ifreq)

/* get memdump string (GENERIC) */
#define SIOC_WANPIPE_DUMP	_IOWR('i', 153, struct ifreq)


/* clocking options */
#define	WAN_EXTERNAL	0
#define	WAN_INTERNAL	1

/* intercace options */
#define	WAN_RS232	0
#define	WAN_V35	1

#define WAN_UDP_FAILED_CMD	0xCF
#define WAN_UDP_INVALID_CMD	0xCE
#define WAN_UDP_TIMEOUT_CMD	0xAA
#define WAN_UDP_INVALID_NET_CMD     0xCD

#define	WAN_NO	0
#define	WAN_YES	1

/* UDP Packet Management */
#define UDP_PKT_FRM_STACK	0x00
#define UDP_PKT_FRM_NETWORK	0x01

#define	WANCONFIG_FR	102	/* frame relay link */
#define	WANCONFIG_PPP	103	/* synchronous PPP link */
#define WANCONFIG_CHDLC	104	/* Cisco HDLC Link */
#define WANCONFIG_AFT    117	/* AFT Hardware Support */
/****** Data Types **********************************************************/


/* Front-End status */
enum fe_status {
	FE_UNITIALIZED = 0x00,
	FE_DISCONNECTED,
	FE_CONNECTED
};

/* 'state' defines */
enum wan_states
{
	WAN_UNCONFIGURED,	/* link/channel is not configured */
	WAN_DISCONNECTED,	/* link/channel is disconnected */
	WAN_CONNECTING,		/* connection is in progress */
	WAN_CONNECTED,		/* link/channel is operational */
	WAN_LIMIT,		/* for verification only */
	WAN_DUALPORT,		/* for Dual Port cards */
	WAN_DISCONNECTING,
	WAN_FT1_READY		/* FT1 Configurator Ready */
};

/* 'modem_status' masks */
#define	WAN_MODEM_CTS	0x0001	/* CTS line active */
#define	WAN_MODEM_DCD	0x0002	/* DCD line active */
#define	WAN_MODEM_DTR	0x0010	/* DTR line active */
#define	WAN_MODEM_RTS	0x0020	/* RTS line active */

typedef struct wan_conf {
	char	devname[IFNAMSIZ+1];
	void*	arg;
} wan_conf_t;


/* IOCTL numbers (up to 16) */

#define TRACE_ALL                       0x00
#define TRACE_PROT			0x01
#define TRACE_DATA			0x02

/* values for request/reply byte */
#define UDPMGMT_REQUEST	0x01
#define UDPMGMT_REPLY	0x02
#define UDP_OFFSET	12

#define MAX_FT1_RETRY	100

/* General Critical Flags */
enum {
	SEND_CRIT,
	PERI_CRIT,
	RX_CRIT,
	PRIV_CRIT
};

/*
 * Data structures for IOCTL calls.
 */

typedef struct sdla_dump {	/* WANPIPE_DUMP */
	unsigned long	magic;	/* for verification */
	unsigned long	offset;	/* absolute adapter memory address */
	unsigned long	length;	/* block length */
	void*		ptr;	/* -> buffer */
} sdla_dump_t;

typedef struct sdla_exec {	/* WANPIPE_EXEC */
	unsigned long	magic;	/* for verification */
	void*		cmd;	/* -> command structure */
	void*		data;	/* -> data buffer */
} sdla_exec_t;

#define TRC_INCOMING_FRM	0x00
#define TRC_OUTGOING_FRM	0x01
typedef struct {
	unsigned char	status;
	unsigned char	data_avail;
	unsigned short	real_length;
	unsigned short	time_stamp;
	unsigned long	sec;
	unsigned long	usec;
	unsigned char	data[0];
} wan_trace_pkt_t;

typedef struct wan_trace {
	unsigned long	tracing_enabled;
	struct ifqueue	ifq;
	unsigned int	trace_timeout;
	unsigned int	max_trace_queue;
} wan_trace_t;


/********************************************************
 *	GLOBAL DEFINITION FOR SANGOMA UDP STRUCTURE	*
 *******************************************************/
#define GLOBAL_UDP_SIGNATURE		"WANPIPE"
#define GLOBAL_UDP_SIGNATURE_LEN	7
#define UDPMGMT_UDP_PROTOCOL		0x11
#define WAN_UDP_CMD_START	0x60
#define WAN_GET_PROTOCOL	(WAN_UDP_CMD_START+0)
#define WAN_GET_PLATFORM	(WAN_UDP_CMD_START+1)
#define WAN_GET_MEDIA_TYPE	(WAN_UDP_CMD_START+2)
#define WAN_UDP_CMD_END		0x6F

#define WAN_FE_CMD_START	0x90
#define WAN_FE_CMD_END		0x9F

#define WAN_INTERFACE_CMD_START	0xA0
#define WAN_INTERFACE_CMD_END	0xAF

#define WAN_FE_UDP_CMD_START	0xB0
#define WAN_FE_UDP_CMD_END	0xBF

typedef struct {
	unsigned char	signature[8];
	unsigned char	request_reply;
	unsigned char	id;
	unsigned char	reserved[6];
} wan_mgmt_t;


/****** DEFINITION OF UDP HEADER AND STRUCTURE PER PROTOCOL ******/
typedef struct {
	unsigned char	num_frames;
	unsigned char	ismoredata;
} wan_trace_info_t;

typedef struct wan_udp_hdr{
	wan_mgmt_t	wan_mgmt;
	wan_cmd_t	wan_cmd;
	union {
		struct {
			wan_trace_info_t	trace_info;
			unsigned char		data[WAN_MAX_DATA_SIZE];
		} chdlc, aft;
		unsigned char data[WAN_MAX_DATA_SIZE];
	} wan_udphdr_u;
#define wan_udphdr_signature		wan_mgmt.signature
#define wan_udphdr_request_reply	wan_mgmt.request_reply
#define wan_udphdr_id			wan_mgmt.id
#define wan_udphdr_opp_flag		wan_cmd.wan_cmd_opp_flag
#define wan_udphdr_command		wan_cmd.wan_cmd_command
#define wan_udphdr_data_len		wan_cmd.wan_cmd_data_len
#define wan_udphdr_return_code		wan_cmd.wan_cmd_return_code
#define wan_udphdr_chdlc_num_frames	wan_udphdr_u.chdlc.trace_info.num_frames
#define wan_udphdr_chdlc_ismoredata	wan_udphdr_u.chdlc.trace_info.ismoredata
#define wan_udphdr_chdlc_data		wan_udphdr_u.chdlc.data

#define wan_udphdr_aft_num_frames	wan_udphdr_u.aft.trace_info.num_frames
#define wan_udphdr_aft_ismoredata	wan_udphdr_u.aft.trace_info.ismoredata
#define wan_udphdr_aft_data		wan_udphdr_u.aft.data
#define wan_udphdr_data			wan_udphdr_u.data
} wan_udp_hdr_t;

#define MAX_LGTH_UDP_MGNT_PKT 2000

/* This is used for interrupt testing */
#define INTR_TEST_MODE	0x02

#define	WUM_SIGNATURE_L	0x50495046
#define	WUM_SIGNATURE_H	0x444E3845

#define	WUM_KILL	0x50
#define	WUM_EXEC	0x51


#if defined(_KERNEL)
/****** Kernel Interface ****************************************************/


#define MAX_E1_CHANNELS 32
#define MAX_FR_CHANNELS (991+1)

#ifndef	min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef	max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#define	is_digit(ch) (((ch)>=(unsigned)'0'&&(ch)<=(unsigned)'9')?1:0)

#define	is_alpha(ch) ((((ch)>=(unsigned)'a'&&(ch)<=(unsigned)'z')||	\
		((ch)>=(unsigned)'A'&&(ch)<=(unsigned)'Z'))?1:0)

#define	is_hex_digit(ch) ((((ch)>=(unsigned)'0'&&(ch)<=(unsigned)'9')||	\
		((ch)>=(unsigned)'a'&&(ch)<=(unsigned)'f')||\
		((ch)>=(unsigned)'A'&&(ch)<=(unsigned)'F'))?1:0)
#if !defined(offsetof)
# define offsetof(type, member)	((size_t)(&((type*)0)->member))
#endif

# define irqreturn_t	void
/* Unsafe sprintf and vsprintf function removed from the kernel */
# define WAN_IRQ_RETVAL(a)		return;

#define	_bit_byte(bit) ((bit) >> 3)
#define	_bit_mask(bit) (1 << ((bit)&0x7))

/* is bit N of bitstring name set? */
#define	bit_test(name, bit) ((name)[_bit_byte(bit)] & _bit_mask(bit))

/* set bit N of bitstring name */
#define	bit_set(name, bit) ((name)[_bit_byte(bit)] |= _bit_mask(bit))

/* clear bit N of bitstring name */
#define	bit_clear(name, bit) ((name)[_bit_byte(bit)] &= ~_bit_mask(bit))

/* Sangoma assert macro */
#define SAN_ASSERT(a)						\
	if (a){							\
		log(LOG_INFO, "%s:%d: Critical Error!\n",	\
				__FUNCTION__,__LINE__);		\
		return (EINVAL);				\
	}

/****** Data Structures *****************************************************/

typedef struct wan_udp_pkt {
	struct ip	ip_hdr;
	struct udphdr	udp_hdr;
	wan_udp_hdr_t	wan_udp_hdr;
#define wan_udp_cmd			wan_udp_hdr.wan_cmd
#define wan_udp_signature		wan_udp_hdr.wan_udphdr_signature
#define wan_udp_request_reply		wan_udp_hdr.wan_udphdr_request_reply
#define wan_udp_id			wan_udp_hdr.wan_udphdr_id
#define wan_udp_opp_flag		wan_udp_hdr.wan_udphdr_opp_flag
#define wan_udp_command			wan_udp_hdr.wan_udphdr_command
#define wan_udp_data_len		wan_udp_hdr.wan_udphdr_data_len
#define wan_udp_return_code		wan_udp_hdr.wan_udphdr_return_code
#define wan_udp_hdlc_PF_bit		wan_udp_hdr.wan_udphdr_hdlc_PF_bit
#define wan_udp_fr_dlci			wan_udp_hdr.wan_udphdr_fr_dlci
#define wan_udp_fr_attr			wan_udp_hdr.wan_udphdr_fr_attr
#define wan_udp_fr_rxlost1		wan_udp_hdr.wan_udphdr_fr_rxlost1
#define wan_udp_fr_rxlost2		wan_udp_hdr.wan_udphdr_fr_rxlost2
#define wan_udp_chdlc_num_frames	wan_udp_hdr.wan_udphdr_chdlc_num_frames
#define wan_udp_chdlc_ismoredata	wan_udp_hdr.wan_udphdr_chdlc_ismoredata
#define wan_udp_chdlc_data		wan_udp_hdr.wan_udphdr_chdlc_data

#define wan_udp_aft_num_frames		wan_udp_hdr.wan_udphdr_aft_num_frames
#define wan_udp_aft_ismoredata		wan_udp_hdr.wan_udphdr_aft_ismoredata
#define wan_udp_data			wan_udp_hdr.wan_udphdr_data
} wan_udp_pkt_t;

#define WAN_IFP_TO_COMMON(ifp)	(wanpipe_common_t*)((ifp)->if_softc)
typedef struct wanpipe_common {
	struct sppp	ifp;
	void		*card;
	struct timeout	dev_timer;
	unsigned int	protocol;
	struct ifmedia	ifm;

	LIST_ENTRY(wanpipe_common)	next;
} wanpipe_common_t;

typedef struct {
	unsigned long	time_slot_map;
	unsigned long	logic_ch_map;
	unsigned char	num_of_time_slots;
	unsigned char	top_logic_ch;
	unsigned long	bar;
	void		*trace_info;
	void		*dev_to_ch_map[MAX_E1_CHANNELS];
	void		*rx_dma_ptr;
	void		*tx_dma_ptr;
	unsigned short	num_of_ch;/* Number of logical channels */
	unsigned short	dma_per_ch;/* DMA buffers per logic channel */
	unsigned short	mru_trans;/* MRU of transparent channels */
	unsigned long	dma_mtu_off;
	unsigned short	dma_mtu;
	unsigned char	state_change_exit_isr;
	unsigned long	active_ch_map;
	unsigned long	fifo_addr_map;
	struct timeout	led_timer;
} sdla_xilinx_t;

/* Adapter Data Space.
 * This structure is needed because we handle multiple cards, otherwise
 * static data would do it.
 */
typedef struct sdla {
	unsigned	magic;
	char		devname[IFNAMSIZ+1];	/* card name */
	void		*hw;			/* hw configuration */
	unsigned int	type;			/* adapter type */
	unsigned char	line_idle;

	char		state;		/* device state */
	unsigned long	critical;	/* critical section flag */

	int(*iface_up) (struct ifnet*);
	int(*iface_down) (struct ifnet*);
	int(*iface_send) (struct mbuf* skb, struct ifnet*);
	int(*iface_ioctl) (struct ifnet*, int, struct ifreq*);

	unsigned long	state_tick;	/* link state timestamp */
	unsigned long	in_isr;		/* interrupt-in-service flag */
	unsigned long	configured;	/* configurations status */
	int(*del_if) (struct sdla*, struct ifnet*);
	void(*isr)(struct sdla*);	/* interrupt service routine */
	void(*poll)(struct sdla*);	/* polling routine */
	int(*exec)(struct sdla*, void*, void*);
	int(*ioctl) (struct ifnet*, int, struct ifreq*);

	union {
		sdla_xilinx_t	xilinx;
	} u;

	sdla_fe_iface_t	fe_iface;
	union {
#define fe_te	u_fe.te_sc
		sdla_te_softc_t	te_sc;
	} u_fe;

	unsigned char		front_end_status;
	WRITE_FRONT_END_REG_T*	write_front_end_reg;
	READ_FRONT_END_REG_T*	read_front_end_reg;
	void(*te_enable_timer) (void*);
	void(*te_link_state)  (void*);

	LIST_HEAD(,wanpipe_common)	dev_head;
	LIST_ENTRY(sdla)		next;	/* -> next device */
} sdla_t;

/****** Public Functions ****************************************************/

void*		wan_xilinx_init(sdla_t*);	/* Xilinx Hardware Support */
struct mbuf*	wan_mbuf_alloc(int);
int 		wan_mbuf_to_buffer(struct mbuf**);

#endif	/* __KERNEL__ */
#endif	/* __IF_SAN_COMMON_H */

/*	$OpenBSD: mnode.h,v 1.13 2010/03/07 13:42:15 miod Exp $ */

/*
 * Copyright (c) 2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __MACHINE_MNODE_H__
#define __MACHINE_MNODE_H__

/*
 * Definitions for Nodes set up in M-Mode. Some stuff here
 * inspired by information gathered from Linux source code.
 */

/*
 * IP27 uses XKSSEG to access the 1TB memory area.
 */
#define	IP27_CAC_BASE	PHYS_TO_XKPHYS(0, CCA_CACHED)

/*
 * IP27 uses XKPHYS space for accessing special objects.
 * Note that IP27_UNCAC_BASE is a linear space without specials.
 */
#define	IP27_HSPEC_BASE	PHYS_TO_XKPHYS_UNCACHED(0, SP_HUB)
#define	IP27_IO_BASE	PHYS_TO_XKPHYS_UNCACHED(0, SP_IO)
#define	IP27_MSPEC_BASE	PHYS_TO_XKPHYS_UNCACHED(0, SP_SPECIAL)
#define	IP27_UNCAC_BASE	PHYS_TO_XKPHYS_UNCACHED(0, SP_NC)

/*
 * Macros used to find the base of each nodes address space.
 * In M mode each node space is 4GB; in N mode each node space is 2GB only.
 */
#define IP27_NODE_BASE(base, node) ((base) + ((uint64_t)(node) << kl_n_shift))
#define	IP27_NODE_SIZE_MASK	   ((1UL << kl_n_shift) - 1)

#define IP27_NODE_CAC_BASE(node)	(IP27_NODE_BASE(IP27_CAC_BASE, node))
#define IP27_NODE_HSPEC_BASE(node)	(IP27_NODE_BASE(IP27_HSPEC_BASE, node))
#define IP27_NODE_IO_BASE(node)		(IP27_NODE_BASE(IP27_IO_BASE, node))
#define IP27_NODE_MSPEC_BASE(node)	(IP27_NODE_BASE(IP27_MSPEC_BASE, node))
#define IP27_NODE_UNCAC_BASE(node)	(IP27_NODE_BASE(IP27_UNCAC_BASE, node))

/* Get typed address to nodes uncached space */
#define IP27_UNCAC_ADDR(type, node, offs) \
	((type)(IP27_NODE_UNCAC_BASE(node) + ((offs) & IP27_NODE_SIZE_MASK)))

/*
 * IP27 platforms uses something called kldir to describe each
 * nodes configuration. Directory entries looks like:
 */
#define IP27_KLDIR_MAGIC	0x434d5f53505f5357

typedef struct kldir_entry {
	uint64_t	 magic;
	off_t		 offset;	/* Offset from start of node space */
	void		*pointer;
	size_t		 size;		/* Size in bytes */
	uint64_t	 count;		/* # of entries if array, 1 if not */
	size_t		 stride;	/* Stride if array, 0 if not */
	char		 rsvd[16];	/* Pad entry to 0x40 bytes */
} kldir_entry_t;

/* Get address to a specific directory entry */
#define IP27_KLD_BASE(node)	IP27_UNCAC_ADDR(kldir_entry_t *, node, 0x2000)
#define IP27_KLD_LAUNCH(node)		(IP27_KLD_BASE(node) + 0)
#define IP27_KLD_KLCONFIG(node)		(IP27_KLD_BASE(node) + 1)
#define IP27_KLD_NMI(node)		(IP27_KLD_BASE(node) + 2)
#define IP27_KLD_GDA(node)		(IP27_KLD_BASE(node) + 3)
#define IP27_KLD_FREEMEM(node)		(IP27_KLD_BASE(node) + 4)
#define IP27_KLD_SYMMON_STK(node)	(IP27_KLD_BASE(node) + 5)
#define IP27_KLD_PI_ERROR(node)		(IP27_KLD_BASE(node) + 6)
#define IP27_KLD_KERN_VARS(node)	(IP27_KLD_BASE(node) + 7)
#define IP27_KLD_KERN_XP(node)		(IP27_KLD_BASE(node) + 8)
#define IP27_KLD_KERN_PARTID(node)	(IP27_KLD_BASE(node) + 9)

/* ========== */

/*
 * KLCONFIG is a linked list of data structures describing the
 * system configuration. 
 */
typedef uint32_t klconf_off_t;
typedef char confidence_t;
 
typedef struct console_s {
	unsigned long	uart_base;
	unsigned long	config_base;
	unsigned long	memory_base;
	short		baud;
	short		flag;
	int		type;
	int16_t		nasid;
	char		wid;
	char		npci;
	uint64_t	baseio_nic;
} console_t;

typedef struct klc_malloc_hdr {
	klconf_off_t km_base;
	klconf_off_t km_limit;
	klconf_off_t km_current;
} klc_malloc_hdr_t;

/* KLCONFIG header addressed by IP27_KLCONFIG_HDR(node) */
#define IP27_KLCONFIG_HDR(n) \
	IP27_UNCAC_ADDR(kl_config_hdr_t *, n, IP27_KLD_KLCONFIG(n)->offset)

typedef struct kl_config_hdr {
	uint64_t	magic;		/* set this to KLCFGINFO_MAGIC */
	uint32_t	version;	/* structure version number */
	klconf_off_t	malloc_hdr_off;	/* offset of ch_malloc_hdr */
	klconf_off_t	cons_off;	/* offset of ch_cons */
	klconf_off_t	board_info;	/* the link list of boards */
	console_t	cons_info;	/* address info of the console */
	klc_malloc_hdr_t malloc_hdr[3];
	confidence_t	sw_belief;	/* confidence that software is bad */
	confidence_t	sn0net_belief; /* confidence that sn0net is bad */
} kl_config_hdr_t;

/* Board info. */
#define IP27_KLFIRST_BOARD(n) \
	IP27_UNCAC_ADDR(lboard_t *, n, IP27_KLCONFIG_HDR(n)->board_info)
#define IP27_KLNEXT_BOARD(n, board) \
	IP27_UNCAC_ADDR(lboard_t *, n, board->brd_next)
#define	MAX_COMPTS_PER_BRD	24

typedef struct lboard_s {
	klconf_off_t	brd_next;	/* Next BOARD */
	uint8_t		struct_type;	/* type, local or remote */
	unsigned char	brd_type;	/* type+class */
	unsigned char	brd_sversion;	/* version of this structure */
	unsigned char	brd_brevision;	/* board revision */
	unsigned char	brd_promver;	/* board prom version, if any */
	unsigned char	brd_flags;	/* Enabled, Disabled etc */
	unsigned char	brd_slot;	/* slot number */
	unsigned short	brd_debugsw;	/* Debug switches */
	short		brd_module;	/* module to which it belongs */
	char		brd_partition;	/* Partition number */
	unsigned short	brd_diagval;	/* diagnostic value */
	unsigned short	brd_diagparm;	/* diagnostic parameter */
	unsigned char	brd_inventory;	/* inventory history */
	unsigned char	brd_numcompts;	/* Number of components */
	uint64_t	brd_nic;	/* Number in CAN */
	int16_t		brd_nasid;	/* passed parameter */
	klconf_off_t	brd_compts[MAX_COMPTS_PER_BRD]; /* COMPONENTS */
	klconf_off_t	brd_errinfo;	/* Board's error information */
	struct lboard_s *brd_parent;	/* Logical parent for this brd */
	uint32_t	brd_graph_link;	/* vertex hdl to connect extrn compts */
	confidence_t	brd_confidence;	/* confidence that the board is bad */
	int16_t		brd_owner;	/* who owns this board */
	uint8_t		brd_nic_flags;	/* To handle 8 more NICs */
	char		brd_name[32];
} lboard_t;

#define	LBOARD	0x01
#define	RBOARD	0x02

/* Definitions of board type and class */
#define	IP27_BC_MASK	0xf0
#define	IP27_BC_NODE	0x10
#define	IP27_BC_IO	0x20
#define	IP27_BC_ROUTER	0x30
#define	IP27_BC_MPLANE	0x40
#define	IP27_BC_GRAF	0x50
#define	IP27_BC_HDTV	0x60
#define	IP27_BC_BRICK	0x70

#define	IP27_BT_MASK	0x0f
#define	IP27_BT_CPU	0x01
#define	IP27_BT_BASEIO	0x01
#define	IP27_BT_MPLANE8	0x01

#define	IP27_BRD_BASEIO		(IP27_BC_IO | IP27_BT_BASEIO)
#define	IP27_BRD_IBRICK		(IP27_BC_BRICK | 0x01)
#define	IP27_BRD_PBRICK		(IP27_BC_BRICK | 0x02)
#define	IP27_BRD_XBRICK		(IP27_BC_BRICK | 0x03)
#define	IP27_BRD_NBRICK		(IP27_BC_BRICK | 0x04)
#define	IP27_BRD_PEBRICK	(IP27_BC_BRICK | 0x05)
#define	IP27_BRD_PXBRICK	(IP27_BC_BRICK | 0x06)
#define	IP27_BRD_IXBRICK	(IP27_BC_BRICK | 0x07)
#define	IP27_BRD_CGBRICK	(IP27_BC_BRICK | 0x08)


/* Component info. Common info about a component. */
typedef struct klinfo_s {			/* Generic info */
	unsigned char	struct_type;	  /* type of this structure */
	unsigned char	struct_version;   /* version of this structure */
	unsigned char	flags;		  /* Enabled, disabled etc */
	unsigned char	revision;	  /* component revision */
	unsigned short	diagval;	  /* result of diagnostics */
	unsigned short	diagparm;	  /* diagnostic parameter */
	unsigned char	inventory;	  /* previous inventory status */
	uint64_t	nic;		  /* Must be aligned properly */
	unsigned char	physid;		  /* physical id of component */
	unsigned int	virtid;		  /* virtual id as seen by system */
	unsigned char	widid;		  /* Widget id - if applicable */
	int16_t		nasid;		  /* node number - from parent */
	char		pad1;		  /* pad out structure. */
	char		pad2;		  /* pad out structure. */
	void		*arcs_compt;	  /* ptr to the arcs struct for ease*/
	klconf_off_t	errinfo;	  /* component specific errors */
	unsigned short	pad3;		  /* pci fields have moved over to */
	unsigned short	pad4;		  /* klbri_t */
} klinfo_t;

#define	KLINFO_ENABLED	0x01
#define	KLINFO_FAILED	0x02

/*
 * Component structure types.
 */
#define	KLSTRUCT_UNKNOWN	0
#define	KLSTRUCT_CPU		1
#define	KLSTRUCT_HUB		2
#define	KLSTRUCT_MEMBNK		3
#define	KLSTRUCT_XBOW		4
#define	KLSTRUCT_BRI		5
#define	KLSTRUCT_IOC3		6
#define	KLSTRUCT_PCI		7
#define	KLSTRUCT_VME		8
#define	KLSTRUCT_ROU		9
#define	KLSTRUCT_GFX		10
#define	KLSTRUCT_SCSI		11
#define	KLSTRUCT_FDDI		12
#define	KLSTRUCT_MIO		13
#define	KLSTRUCT_DISK		14
#define	KLSTRUCT_TAPE		15
#define	KLSTRUCT_CDROM		16
#define	KLSTRUCT_HUB_UART	17
#define	KLSTRUCT_IOC3ENET	18
#define	KLSTRUCT_IOC3UART	19
#define	KLSTRUCT_UNUSED		20
#define	KLSTRUCT_IOC3PCKM	21
#define	KLSTRUCT_RAD		22
#define	KLSTRUCT_HUB_TTY	23
#define	KLSTRUCT_IOC3_TTY	24
/* new IP35 values */
#define	KLSTRUCT_FIBERCHANNEL	25
#define	KLSTRUCT_MOD_SERIAL_NUM	26
#define	KLSTRUCT_IOC3MS		27
#define	KLSTRUCT_TPU		28
#define	KLSTRUCT_GSN_A		29
#define	KLSTRUCT_GSN_B		30
#define	KLSTRUCT_XTHD		31
#define	KLSTRUCT_QLFIBRE	32
#define	KLSTRUCT_FIREWIRE	33
#define	KLSTRUCT_USB		34
#define	KLSTRUCT_USBKBD		35
#define	KLSTRUCT_USBMS		36
#define	KLSTRUCT_SCSI2		37
#define	KLSTRUCT_PEBRICK	38
#define	KLSTRUCT_GIGENET	39
#define	KLSTRUCT_IDE		40

typedef struct klport_s {
	int16_t		port_nasid;
	unsigned char	port_flag;
	klconf_off_t	port_offset;
} klport_t;

/* KLSTRUCT_CPU: CPU component info */
typedef struct klcpu_s {
	klinfo_t	cpu_info;
	uint16_t	cpu_prid;	/* Processor PRID value */
	uint16_t	cpu_fpirr;	/* FPU IRR value */
	uint16_t	cpu_speed;	/* Speed in MHZ */
	uint16_t	cpu_scachesz;	/* secondary cache size in MB */
	uint16_t	cpu_scachespeed;/* secondary cache speed in MHz */
} klcpu_t;

/* KLSTRUCT_HUB: Hub */
typedef struct klhub_s {
	klinfo_t	hub_info;
	uint32_t	hub_flags;	/* PCFG_HUB_xxx flags */
	klport_t	hub_port;	/* hub is connected to this */
	uint64_t	hub_box_nic;	/* nic of containing box */
	klconf_off_t	hub_mfg_nic;	/* MFG NIC string */
	uint64_t	hub_speed;	/* Speed of hub in HZ */
} klhub_t;

/* KLSTRUCT_MEMBNK: Memory bank */
#define	MD_MEM_BANKS_M	8	/* M-Mode */
typedef struct klmembnk_m_s {
	klinfo_t	membnk_info;
	int16_t		membnk_memsz;		/* Total memory in megabytes */
	int16_t		membnk_dimm_select;	/* bank to phys addr mapping*/
	int16_t		membnk_bnksz[MD_MEM_BANKS_M]; /* Memory bank sizes */
	int16_t		membnk_attr;
} klmembnk_m_t;

#define	MD_MEM_BANKS_N	4	/* N-Mode */
typedef struct klmembnk_n_s {
	klinfo_t	membnk_info;
	int16_t		membnk_memsz;		/* Total memory in megabytes */
	int16_t		membnk_dimm_select;	/* bank to phys addr mapping*/
	int16_t		membnk_bnksz[MD_MEM_BANKS_N]; /* Memory bank sizes */
	int16_t		membnk_attr;
} klmembnk_n_t;

/* KLSTRUCT_XBOW: Xbow */
#define	MAX_XBOW_LINKS	16
typedef struct klxbow_s {
	klinfo_t	xbow_info;
	klport_t	xbow_port_info[MAX_XBOW_LINKS];	/* Module number */
	int		xbow_hub_master_link;
} klxbow_t;

/* xbow_port_info.port_flag bits */
#define	XBOW_PORT_IO		0x01
#define	XBOW_PORT_HUB		0x02
#define	XBOW_PORT_ENABLE	0x04

/* KLSTRUCT_IOC3: Basic I/O Controller */
typedef struct klioc3_s {
	klinfo_t	ioc3_info;
	unsigned char	ioc3_ssram;	/* Info about ssram */
	unsigned char	ioc3_nvram;	/* Info about nvram */
	klinfo_t	ioc3_superio;	/* Info about superio */
	klconf_off_t	ioc3_tty_off;
	klinfo_t	ioc3_enet;
	klconf_off_t	ioc3_enet_off;
	klconf_off_t	ioc3_kbd_off;
} klioc3_t;

/* KLSTRUCT_IOC3_TTY: IOC3 attached TTY */
typedef struct klttydev_s {
	klinfo_t	ttydev_info;
	struct terminal_data *ttydev_cfg;	/* driver fills up this */
} klttydev_t;

/* ========== */

#define IP27_NMI(n) \
	IP27_UNCAC_ADDR(nmi_t *, n, IP27_KLD_NMI(n)->offset)

#define	NMI_MAGIC	0x0048414d4d455201	/* \x00HAMMER\x01 */

typedef struct nmi {
	uint64_t	magic;			/* NMI_MAGIC */
	uint64_t	flags;
	uint64_t	cb;			/* callback function */
	uint64_t	cb_complement;		/* two's complement of above */
	uint64_t	cb_arg;			/* callback arg */
	uint64_t	master;			/* nonzero if master node */
} nmi_t;

/* ========== */

#define IP27_GDA(n)		(gda_t *)(IP27_KLD_GDA(n)->pointer)
#define IP27_GDA_SIZE(n)	(IP27_KLD_GDA(n)->size)

#define	GDA_MAGIC	0x58464552		/* XFER */

#define	GDA_MAXNODES	128

typedef struct gda {
	uint32_t	 magic;			/* GDA_MAGIC */
	uint16_t	 ver;
	uint16_t	 masternasid;		/* NASID of the master cpu */
	uint32_t	 promop;		/* Request to pass to PROM */
	uint32_t	 switches;
	void		*tlb_handlers[3];
	uint		 partid;
	uint		 symcnt;
	void		*symtab;
	char		*symnames;
	void		*replication_mask;
	uint32_t	 pad[14];
	int16_t		 nasid[GDA_MAXNODES];	/* NASID of connected nodes */
} gda_t;

#define	GDA_PROMOP_MAGIC	0x0ead0000
/* commands */
#define	GDA_PROMOP_HALT		0x00000010
#define	GDA_PROMOP_POWERDOWN	0x00000020
#define	GDA_PROMOP_RESTART	0x00000030
#define	GDA_PROMOP_REBOOT	0x00000040
#define	GDA_PROMOP_EIM		0x00000050
/* options */
#define	GDA_PROMOP_NO_DIAGS	0x00000100	/* don't run diagnostics */
#define	GDA_PROMOP_NO_MEMINIT	0x00000200	/* don't initialize memory */
#define	GDA_PROMOP_NO_DEVINIT	0x00000400	/* don't initialize devices */

/* ========== */

/*
 * Functions.
 */

console_t *kl_get_console(void);
void	kl_init(int);
void	kl_scan_config(int);
int	kl_scan_node(int, uint, int (*)(lboard_t *, void *), void *);
#define	KLBRD_ANY	0
int	kl_scan_board(lboard_t *, uint, int (*)(klinfo_t *, void *), void *);
#define	KLSTRUCT_ANY	((uint)~0)

extern int kl_n_mode;
extern u_int kl_n_shift;
extern klinfo_t *kl_glass_console;

#endif /* __MACHINE_MNODE_H__ */

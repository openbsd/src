/*
 * Copyright (c) 1993, 1994 Stefan Grefen.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following dipclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: pcmciabus.h,v 1.1 1996/01/15 00:05:13 hvozda Exp $
 */
 /* derived from scsicconf.[ch] writenn by Julian Elischer et al */

#ifndef	_PCMCIA_PCMCIABUS_H_
#define _PCMCIA_PCMCIABUS_H_ 1

#include <sys/queue.h>
#include <sys/select.h>
#include <machine/cpu.h>

/*
 * The following documentation tries to describe the relationship between the
 * various structures defined in this file:
 *
 * each adapter type has a pcmcia_adapter struct. This describes the adapter and
 *    identifies routines that can be called to use the adapter.
 * each device type has a pcmcia_device struct. This describes the device and
 *    identifies routines that can be called to use the device.
 * each existing device position (pcmciabus + port)
 *    can be described by a pcmcia_link struct.
 *    Only port positions that actually have devices, have a pcmcia_link
 *    structure assigned. so in effect each device has pcmcia_link struct.
 *    The pcmcia_link structure contains information identifying both the
 *    device driver and the adapter driver for that port on that pcmcia bus,
 *    and can be said to 'link' the two.
 * each individual pcmcia bus has an array that points to all the pcmcia_link
 *    structs associated with that pcmcia bus. Slots with no device have
 *    a NULL pointer.
 * each individual device also knows the address of it's own pcmcia_link
 *    structure.
 *
 *				-------------
 *
 * The key to all this is the pcmcia_link structure which associates all the 
 * other structures with each other in the correct configuration.  The
 * pcmcia_link is the connecting information that allows each part of the 
 * pcmcia system to find the associated other parts.
 */


struct pcmcia_link;
struct pcmcia_conf;
struct pcmcia_adapter;

/*
 * These entrypoints are called by the high-end drivers to get services from
 * whatever low-end drivers they are attached to each adapter type has one of
 * these statically allocated.
 */
struct pcmcia_funcs {
/* 4 map io range */
	int (*pcmcia_map_io) __P((struct pcmcia_link *, u_int, u_int, int));
/* 8 map memory window */
	int (*pcmcia_map_mem) __P((struct pcmcia_link *, caddr_t,
				   u_int, u_int, int));
/*12 map interrupt */
	int (*pcmcia_map_intr) __P((struct pcmcia_link *, int, int));
/*26 power on/off etc */
	int (*pcmcia_service) __P((struct pcmcia_link *, int, void *, int));
};

struct pcmciabus_link {			/* Link back to the bus we are on */
	/* Bus specific configure    */
	int (*bus_config) __P((struct pcmcia_link *, struct device *,
			       struct pcmcia_conf *, struct cfdata *));
	/* Bus specific unconfigure  */
	int (*bus_unconfig) __P((struct pcmcia_link *));
	/* Bus specific probe        */
	int (*bus_probe) __P((struct device *, void *,
			      void *, struct pcmcia_link *));
	/* Bus specific search	     */
	int (*bus_search) __P((struct device *, void *, cfprint_t));
	/* initialize scratch        */
	int (*bus_init) __P((struct device *, struct cfdata *,
			     void *, struct pcmcia_adapter *, int));
};
struct pcmcia_adapter {
	struct pcmcia_funcs *chip_link;
	struct pcmciabus_link *bus_link;
        void *          adapter_softc;
	caddr_t scratch_mem;		/* pointer to scratch window */
	int scratch_memsiz;		/* size of scratch window    */
	int scratch_inuse;		/* window in use             */
};

#define PCMCIA_MAP_ATTR		0x0100 /* for memory only */
#define PCMCIA_MAP_8		0x0100 /* for io only */
#define PCMCIA_MAP_16		0x0200
#define PCMCIA_UNMAP		0x0400
#define PCMCIA_PHYSICAL_ADDR    0x0800
#define PCMCIA_UNMAP_ALL	0x0c00
#define PCMCIA_FIXED_WIN    	0x1000
#define PCMCIA_LAST_WIN	        0x0010
#define PCMCIA_FIRST_WIN	0x0020
#define PCMCIA_ANY_WIN		0x0030

#define	PCMCIA_OP_RESET	    	0x0000
#define	PCMCIA_OP_POWER  	0x0001
#define	PCMCIA_OP_STATUS  	0x0002
#define	PCMCIA_OP_GETREGS  	0x0003
#define	PCMCIA_OP_WAIT  	0x0004

#define PCMCIA_POWER_ON		0x0001
#define PCMCIA_POWER_5V		0x0002
#define PCMCIA_POWER_3V		0x0004
#define PCMCIA_POWER_AUTO	0x0008

#define PCMCIA_CARD_PRESENT     0x0001
#define PCMCIA_BATTERY		0x0002
#define PCMCIA_WRITE_PROT	0x0004
#define PCMCIA_READY		0x0008
#define PCMCIA_POWER		0x0010
#define PCMCIA_POWER_PP		0x0020
#define PCMCIA_CARD_IS_MAPPED   0x1000
#define PCMCIA_CARD_INUSE       0x2000


/*
 * This structure describes the connection between an adapter driver and
 * a device driver, and is used by each to call services provided by
 * the other, and to allow generic pcmcia glue code to call these services
 * as well.
 */
struct pcmcia_link {
       	char	pcmciabus;		/* the Nth pcmciabus */
       	char	slot;			/* slot of this dev */
       	char	flags;			
#define CARD_IS_MAPPED         0x01
#define PCMCIA_ATTACH          0x02
#define PCMCIA_REATTACH        0x04
#define PCMCIA_SLOT_INUSE      0x08
#define PCMCIA_ATTACH_TYPE     (PCMCIA_ATTACH|PCMCIA_REATTACH)
#define PCMCIA_SLOT_EVENT	0x80
#define PCMCIA_SLOT_OPEN	0x40
        char	opennings;

	char    iowin;
	char    memwin;
	char    intr;
	char    dummy;
       	struct	pcmcia_adapter *adapter;	/* adapter entry points etc. */
       	struct	pcmciadevs *device;	/* device entry points etc. */
	void    *devp;	    	    	/* pointer to configured device */
       	void	*fordriver;		/* for private use by the driver */
	void	*shuthook;		/* shutdown hook handle */
	struct selinfo	pcmcialink_sel;	/* for select users */
};

/*
 * One of these is allocated and filled in for each pcmcia bus.
 * it holds pointers to allow the pcmcia bus to get to the driver
 * it also has a template entry which is the prototype struct
 * supplied by the adapter driver, this is used to initialise
 * the others, before they have the rest of the fields filled in
 */
struct pcmciabus_softc {
	struct device sc_dev;
	struct pcmcia_link *sc_link[8];
};

struct pcmcia_conf {
    int irq_share:1; 
    int irq_level:1; /* 1 level */
    int irq_pulse:1; /* 1  pulse */
    int irq_vend:1;
    int irq_iock:1;
    int irq_berr:1;
    int irq_nmi:1;
    int iocard:1;
    u_char iowin;
    u_char memwin;
    u_char irq_num;
    u_char cfgtype;
#define CFGENTRYID     0x20
#define CFGENTRYMASK   (CFGENTRYID|(CFGENTRYID-1))
#define DOSRESET       0x40
    int cfg_regmask;
    int irq_mask;
    int cfg_off;
    struct iowin {
	int start;
	int len;
	int flags;
    }io[4];
    struct memwin {
	int start; 
	int caddr;
	int len;
	int flags;
    }mem[4];
    char driver_name[8][4]; /* up to four different functions on a card */
    int  unitid;
    int  cfgid;
};

struct pcmcia_device {
    char *name;
    int (*pcmcia_config) __P((struct pcmcia_link *, struct device *,
			      struct pcmcia_conf *, struct cfdata *));
    int (*pcmcia_probe) __P((struct device *, void *,
			     void *, struct pcmcia_link *));
    int (*pcmcia_insert) __P((struct pcmcia_link *, struct device *,
			      struct cfdata *));
    int	(*pcmcia_remove) __P((struct pcmcia_link *, struct device *));
};

struct pcmciadevs {
        char *devname;
        int flags;              /* 1 show my comparisons during boot(debug) */
#define PC_SHOWME       0x01
        char *manufacturer;
        char *model;
        char *add_inf1;
        char *add_inf2;
        void *param;
        struct pcmcia_device *dev;
};

#ifdef _KERNEL
extern int pcmcia_add_device __P((struct pcmciadevs *));
extern int pcmcia_get_cf __P((struct pcmcia_link *, u_char *, int, int,
			      struct pcmcia_conf *));
extern int pcmcia_targmatch __P((struct device *, struct cfdata *, void *));
#endif

/* in pcmcia_conf.c, available for user space too: */
extern int pcmcia_get_cisver1 __P((struct pcmcia_link *, u_char *, int,
				   char *, char *, char *, char *));
int      parse_cfent  __P((u_char *, int, int, struct pcmcia_conf *));
int      read_cfg_info __P((u_char *, int, struct pcmcia_conf *));
void 	pcmcia_getstr __P((char *buf, u_char **, u_char *));

#endif /* _PCMCIA_PCMCIABUS_H_ */

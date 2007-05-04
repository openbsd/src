/*	$OpenBSD: autoconf.h,v 1.16 2007/05/04 03:44:44 deraadt Exp $	*/
/*	$NetBSD: autoconf.h,v 1.20 1997/05/24 20:03:03 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)autoconf.h	8.2 (Berkeley) 9/30/93
 */

/*
 * Autoconfiguration information.
 */

/*
 * Most devices are configured according to information kept in
 * the FORTH PROMs.  In particular, we extract the `name', `reg',
 * and `address' properties of each device attached to the mainbus;
 * other drives may also use this information.  The mainbus itself
 * (which `is' the CPU, in some sense) gets just the node, with a
 * fake name ("mainbus").
 */

#define	RA_MAXVADDR	8		/* max (virtual) addresses per device */
#define	RA_MAXREG	16		/* max # of register banks per device */
#define	RA_MAXINTR	8		/* max interrupts per device */

struct romaux {
	const char *ra_name;		/* name from FORTH PROM */
	int	ra_node;		/* FORTH PROM node ID */
	void	*ra_vaddrs[RA_MAXVADDR];/* ROM mapped virtual addresses */
	int	ra_nvaddrs;		/* # of ra_vaddrs[]s, may be 0 */
#define ra_vaddr	ra_vaddrs[0]	/* compatibility */

	struct rom_reg {
		int	rr_iospace;	/* register space (obio, etc) */
		void	*rr_paddr;	/* register physical address */
		int	rr_len;		/* register length */
	} ra_reg[RA_MAXREG];
	int	ra_nreg;		/* # of ra_reg[]s */
#define ra_iospace	ra_reg[0].rr_iospace
#define ra_paddr	ra_reg[0].rr_paddr
#define ra_len		ra_reg[0].rr_len

	struct rom_intr {		/* interrupt information: */
		int	int_pri;		/* priority (IPL) */
		int	int_vec;		/* vector (always 0?) */
	} ra_intr[RA_MAXINTR];
	int	ra_nintr;		/* number of interrupt info elements */

	struct	bootpath *ra_bp;	/* used for locating boot device */
};

struct rom_range {		/* Only used on v3 PROMs */
	u_int32_t	cspace;		/* Client space */
	u_int32_t	coffset;	/* Client offset */
	u_int32_t	pspace;		/* Parent space */
	u_int32_t	poffset;	/* Parent offset */
	u_int32_t	size;		/* Size in bytes of this range */
};


struct confargs {
	int	ca_bustype;
	struct	romaux ca_ra;
	int	ca_slot;
	int	ca_offset;
};
#define BUS_MAIN	0
#define BUS_OBIO	1
#define BUS_VME16	2
#define BUS_VME32	3
#define BUS_SBUS	4
#define BUS_XBOX	5
#define BUS_FGA		6
#define BUS_FGA_A16D8	7
#define BUS_FGA_A16D16	8
#define BUS_FGA_A16D32	9
#define BUS_FGA_A24D8	10
#define BUS_FGA_A24D16	11
#define BUS_FGA_A24D32	12
#define BUS_FGA_A32D8	13
#define BUS_FGA_A32D16	14
#define BUS_FGA_A32D32	15

/*
 * mapiodev maps an I/O device to a virtual address, returning the address.
 * mapdev does the real work: you can supply a special virtual address and
 * it will use that instead of creating one, but you must only do this if
 * you get it from ../sparc/vaddrs.h.
 */
void	*mapdev(struct rom_reg *pa, int va, int offset, int size);
#define	mapiodev(pa, offset, size) \
	mapdev(pa, 0, offset, size)
/*
 * REG2PHYS is provided for drivers with a `d_mmap' function.
 */
#define REG2PHYS(rr, offset) \
	(((u_int)(rr)->rr_paddr + (offset)) | PMAP_IOENC((rr)->rr_iospace) )

/* For VME and sun4/obio busses */
void	*bus_map(struct rom_reg *, int);
void	bus_untmp(void);

/*
 * The various getprop* functions obtain `properties' from the ROMs.
 * getprop() obtains a property as a byte-sequence, and returns its
 * length; the others convert or make some other guarantee.
 */
int	getproplen(int node, char *name);
int	getprop(int node, char *name, void *buf, int bufsiz);
char	*getpropstring(int node, char *name);
int	getpropint(int node, char *name, int deflt);

/* Frequently used options node */
extern int optionsnode;
/* Machine type */
extern char mainbus_model[30];

/*
 * The romprop function gets physical and virtual addresses from the PROM
 * and fills in a romaux.  It returns 1 on success, 0 if the physical
 * address is not available as a "reg" property.
 */
int	romprop(struct romaux *ra, const char *name, int node);

/*
 * `clockfreq' produces a printable representation of a clock frequency
 * (this is just a frill).
 */
char	*clockfreq(int freq);

/*
 * Memory description arrays.  Shared between pmap.c and autoconf.c; no
 * one else should use this (except maybe mem.c, e.g., if we fix the VM to
 * handle discontiguous physical memory).
 */
struct memarr {
	u_int	addr;
	u_int	len;
};
int	makememarr(struct memarr *, int max, int which);
#define	MEMARR_AVAILPHYS	0
#define	MEMARR_TOTALPHYS	1

/* Pass a string to the FORTH interpreter.  May fail silently. */
void	rominterpret(char *);

/* Openprom V2 style boot path */
struct device;
struct bootpath {
	char	name[16];	/* name of this node */
	int	val[3];		/* up to three optional values */
	struct device *dev;	/* device that recognised this component */
};

struct bootpath	*bootpath_store(int, struct bootpath *);
int		sd_crazymap(int);

void	bootstrap(void);
int	firstchild(int);
int	nextsibling(int);
void	callrom(void);
struct device *getdevunit(char *, int);
void	*findzs(int);
int	romgetcursoraddr(int **, int **);
int	findroot(void);
int	findnode(int, const char *);
int	opennode(char *);
int	node_has_property(int, const char *);

/*	$OpenBSD: malloc.h,v 1.76 2005/01/14 21:15:08 mcbride Exp $	*/
/*	$NetBSD: malloc.h,v 1.39 1998/07/12 19:52:01 augustss Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)malloc.h	8.5 (Berkeley) 5/3/95
 */

#ifndef _SYS_MALLOC_H_
#define	_SYS_MALLOC_H_

#define KERN_MALLOC_BUCKETS	1
#define KERN_MALLOC_BUCKET	2
#define KERN_MALLOC_KMEMNAMES	3
#define KERN_MALLOC_KMEMSTATS	4
#define KERN_MALLOC_MAXID	5

#define CTL_KERN_MALLOC_NAMES { \
	{ 0, 0 }, \
	{ "buckets", CTLTYPE_STRING }, \
	{ "bucket", CTLTYPE_NODE }, \
	{ "kmemnames", CTLTYPE_STRING }, \
	{ "kmemstat", CTLTYPE_NODE }, \
}

/*
 * flags to malloc
 */
#define	M_WAITOK	0x0000
#define	M_NOWAIT	0x0001
#define M_CANFAIL	0x0002

/*
 * Types of memory to be allocated
 */
#define	M_FREE		0	/* should be on free list */
#define	M_MBUF		1	/* mbuf */
#define	M_DEVBUF	2	/* device driver memory */
#define M_DEBUG		3	/* debug chunk */
#define	M_PCB		4	/* protocol control block */
#define	M_RTABLE	5	/* routing tables */
/* 6 - free */
#define	M_FTABLE	7	/* fragment reassembly header */
/* 8 - free */
#define	M_IFADDR	9	/* interface address */
#define	M_SOOPTS	10	/* socket options */
#define	M_SYSCTL	11	/* sysctl buffers (persistent storage) */
/* 12 - free */
/* 13 - free */
#define	M_IOCTLOPS	14	/* ioctl data buffer */
/* 15-18 - free */
#define	M_IOV		19	/* large iov's */
#define	M_MOUNT		20	/* vfs mount struct */
/* 21 - free */
#define	M_NFSREQ	22	/* NFS request header */
#define	M_NFSMNT	23	/* NFS mount structure */
#define	M_NFSNODE	24	/* NFS vnode private part */
#define	M_VNODE		25	/* Dynamically allocated vnodes */
#define	M_CACHE		26	/* Dynamically allocated cache entries */
#define	M_DQUOT		27	/* UFS quota entries */
#define	M_UFSMNT	28	/* UFS mount structure */
#define	M_SHM		29	/* SVID compatible shared memory segments */
#define	M_VMMAP		30	/* VM map structures */
#define	M_SEM		31	/* SVID compatible semaphores */
#define	M_DIRHASH	32	/* UFS dirhash */
/* 33 - free */
#define	M_VMPMAP	34	/* VM pmap */
/* 35-37 - free */
#define	M_FILE		38	/* Open file structure */
#define	M_FILEDESC	39	/* Open file descriptor table */
/* 40 - free */
#define	M_PROC		41	/* Proc structures */
#define	M_SUBPROC	42	/* Proc sub-structures */
#define	M_VCLUSTER	43	/* Cluster for VFS */
/* 45-46 - free */
#define	M_MFSNODE	46	/* MFS vnode private part */
/* 47-48 - free */
#define	M_NETADDR	49	/* Export host address structure */
#define	M_NFSSVC	50	/* Nfs server structure */
#define	M_NFSUID	51	/* Nfs uid mapping structure */
#define	M_NFSD		52	/* Nfs server daemon structure */
#define	M_IPMOPTS	53	/* internet multicast options */
#define	M_IPMADDR	54	/* internet multicast address */
#define	M_IFMADDR	55	/* link-level multicast address */
#define	M_MRTABLE	56	/* multicast routing tables */
#define	M_ISOFSMNT	57	/* ISOFS mount structure */
#define	M_ISOFSNODE	58	/* ISOFS vnode private part */
#define	M_MSDOSFSMNT	59	/* MSDOS FS mount structure */
#define	M_MSDOSFSFAT	60	/* MSDOS FS fat table */
#define	M_MSDOSFSNODE	61	/* MSDOS FS vnode private part */
#define	M_TTYS		62	/* allocated tty structures */
#define	M_EXEC		63	/* argument lists & other mem used by exec */
#define	M_MISCFSMNT	64	/* miscfs mount structures */
/* 65 - free */
#define	M_ADOSFSMNT	66	/* adosfs mount structures */
/* 67 - free */
#define	M_ANODE		68	/* adosfs anode structures and tables. */
/* 69-70 - free */
#define	M_ADOSFSBITMAP	71	/* adosfs bitmap */
#define	M_EXT2FSNODE	72	/* EXT2FS vnode private part */
/* 73 - free */
#define	M_PFKEY		74	/* pfkey data */
#define	M_TDB		75	/* Transforms database */
#define	M_XDATA		76	/* IPsec data */
/* 77 - free */
#define	M_PAGEDEP	78	/* File page dependencies */
#define	M_INODEDEP	79	/* Inode dependencies */
#define	M_NEWBLK	80	/* New block allocation */
/* 81-82 - free */
#define	M_INDIRDEP	83	/* Indirect block dependencies */
/* 84-91 - free */
#define M_VMSWAP	92	/* VM swap structures */
/* 93-96 - free */
#define	M_RAIDFRAME	97	/* RAIDframe data */
#define M_UVMAMAP	98	/* UVM amap and related */
#define M_UVMAOBJ	99	/* UVM aobj and related */
/* 100 - free */
#define	M_USB		101	/* USB general */
#define	M_USBDEV	102	/* USB device driver */
#define	M_USBHC		103	/* USB host controller */
/* 104 - free */
#define M_MEMDESC	105	/* Memory range */
#define M_UFS_EXTATTR	106	/* Extended Attributes */
/* 107 - free */
#define M_CRYPTO_DATA	108	/* Crypto framework data buffers (keys etc.) */
/* 109 - free */
#define M_CREDENTIALS	110	/* IPsec-related credentials and ID info */
#define M_PACKET_TAGS	111	/* Packet-attached information */
#define M_1394CTL	112	/* IEEE 1394 control structures */
#define M_1394DATA	113	/* IEEE 1394 data buffers */
#define	M_EMULDATA	114	/* Per-process emulation data */
/* 115-122 - free */

/* KAME IPv6 */
#define	M_IP6OPT	123	/* IPv6 options */
#define	M_IP6NDP	124	/* IPv6 Neighbour Discovery */
#define	M_IP6RR		125	/* IPv6 Router Renumbering Prefix */
#define	M_RR_ADDR	126	/* IPv6 Router Renumbering Ifid */
#define	M_TEMP		127	/* misc temporary data buffers */

#define	M_NTFSMNT	128	/* NTFS mount structure */
#define	M_NTFSNTNODE	129	/* NTFS ntnode information */
#define	M_NTFSFNODE	130	/* NTFS fnode information */
#define	M_NTFSDIR	131	/* NTFS dir buffer */
#define	M_NTFSNTHASH	132	/* NTFS ntnode hash tables */
#define	M_NTFSNTVATTR	133	/* NTFS file attribute information */
#define	M_NTFSRDATA	134	/* NTFS resident data */
#define	M_NTFSDECOMP	135	/* NTFS decompression temporary */
#define	M_NTFSRUN	136	/* NTFS vrun storage */

#define	M_KEVENT	137	/* kqueue related */

#define	M_BLUETOOTH	138	/* Bluetooth */

#define M_BWMETER	139	/* Multicast upcall bw meters */
#define	M_LAST		140	/* Must be last type + 1 */


#define	INITKMEMNAMES { \
	"free",		/* 0 M_FREE */ \
	"mbuf",		/* 1 M_MBUF */ \
	"devbuf",	/* 2 M_DEVBUF */ \
	"debug", 	/* 3 M_DEBUG */ \
	"pcb",		/* 4 M_PCB */ \
	"routetbl",	/* 5 M_RTABLE */ \
	NULL,		/* 6 */ \
	"fragtbl",	/* 7 M_FTABLE */ \
	NULL, \
	"ifaddr",	/* 9 M_IFADDR */ \
	"soopts",	/* 10 M_SOOPTS */ \
	"sysctl",	/* 11 M_SYSCTL */ \
	NULL, \
	NULL, \
	"ioctlops",	/* 14 M_IOCTLOPS */ \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	"iov",		/* 19 M_IOV */ \
	"mount",	/* 20 M_MOUNT */ \
	NULL, \
	"NFS req",	/* 22 M_NFSREQ */ \
	"NFS mount",	/* 23 M_NFSMNT */ \
	"NFS node",	/* 24 M_NFSNODE */ \
	"vnodes",	/* 25 M_VNODE */ \
	"namecache",	/* 26 M_CACHE */ \
	"UFS quota",	/* 27 M_DQUOT */ \
	"UFS mount",	/* 28 M_UFSMNT */ \
	"shm",		/* 29 M_SHM */ \
	"VM map",	/* 30 M_VMMAP */ \
	"sem",		/* 31 M_SEM */ \
	"dirhash",	/* 32 M_DIRHASH */ \
	NULL, \
	"VM pmap",	/* 34 M_VMPMAP */ \
	NULL,	/* 35 */ \
	NULL,	/* 36 */ \
	NULL,	/* 37 */ \
	"file",		/* 38 M_FILE */ \
	"file desc",	/* 39 M_FILEDESC */ \
	NULL,	/* 40 */ \
	"proc",		/* 41 M_PROC */ \
	"subproc",	/* 42 M_SUBPROC */ \
	"VFS cluster",	/* 43 M_VCLUSTER */ \
	NULL, \
	NULL, \
	"MFS node",	/* 46 M_MFSNODE */ \
	NULL, \
	NULL, \
	"Export Host",	/* 49 M_NETADDR */ \
	"NFS srvsock",	/* 50 M_NFSSVC */ \
	"NFS uid",	/* 51 M_NFSUID */ \
	"NFS daemon",	/* 52 M_NFSD */ \
	"ip_moptions",	/* 53 M_IPMOPTS */ \
	"in_multi",	/* 54 M_IPMADDR */ \
	"ether_multi",	/* 55 M_IFMADDR */ \
	"mrt",		/* 56 M_MRTABLE */ \
	"ISOFS mount",	/* 57 M_ISOFSMNT */ \
	"ISOFS node",	/* 58 M_ISOFSNODE */ \
	"MSDOSFS mount", /* 59 M_MSDOSFSMNT */ \
	"MSDOSFS fat",	/* 60 M_MSDOSFSFAT */ \
	"MSDOSFS node",	/* 61 M_MSDOSFSNODE */ \
	"ttys",		/* 62 M_TTYS */ \
	"exec",		/* 63 M_EXEC */ \
	"miscfs mount",	/* 64 M_MISCFSMNT */ \
	NULL, \
	"adosfs mount",	/* 66 M_ADOSFSMNT */ \
	NULL, \
	"adosfs anode",	/* 68 M_ANODE */ \
	NULL, \
	NULL, \
	"adosfs bitmap", /* 71 M_ADOSFSBITMAP */ \
	"EXT2FS node",	/* 72 M_EXT2FSNODE */ \
	NULL, \
	"pfkey data",	/* 74 M_PFKEY */ \
	"tdb",		/* 75 M_TDB */ \
	"xform_data",	/* 76 M_XDATA */ \
	NULL, \
	"pagedep",	/* 78 M_PAGEDEP */ \
	"inodedep",	/* 79 M_INODEDEP */ \
	"newblk",	/* 80 M_NEWBLK */ \
	NULL, \
	NULL, \
	"indirdep",	/* 83 M_INDIRDEP */ \
	NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, \
	"VM swap",	/* 92 M_VMSWAP */ \
	NULL, NULL, NULL, NULL, \
	"RAIDframe data", /* 97 M_RAIDFRAME */ \
	"UVM amap",	/* 98 M_UVMAMAP */ \
	"UVM aobj",	/* 99 M_UVMAOBJ */ \
	NULL, \
	"USB",		/* 101 M_USB */ \
	"USB device",	/* 102 M_USBDEV */ \
	"USB HC",	/* 103 M_USBHC */ \
	NULL, \
	"memdesc",	/* 105 M_MEMDESC */ \
	NULL,	/* 106 */ \
	NULL, \
	"crypto data",	/* 108 M_CRYPTO_DATA */ \
	NULL, \
	"IPsec creds",	/* 110 M_CREDENTIALS */ \
	"packet tags",	/* 111 M_PACKET_TAGS */ \
	"1394ctl",	/* 112 M_1394CTL */ \
	"1394data",	/* 113 M_1394DATA */ \
	"emuldata",	/* 114 M_EMULDATA */ \
	NULL, NULL, NULL, NULL, \
	NULL, NULL, NULL, NULL, \
	"ip6_options",	/* 123 M_IP6OPT */ \
	"NDP",		/* 124 M_IP6NDP */ \
	"ip6rr",	/* 125 M_IP6RR */ \
	"rp_addr",	/* 126 M_RR_ADDR */ \
	"temp",		/* 127 M_TEMP */ \
	"NTFS mount",	/* 128 M_NTFSMNT */ \
	"NTFS node",	/* 129 M_NTFSNTNODE */ \
	"NTFS fnode",	/* 130 M_NTFSFNODE */ \
	"NTFS dir",	/* 131 M_NTFSDIR */ \
	"NTFS hash tables",	/* 132 M_NTFSNTHASH */ \
	"NTFS file attr",	/* 133 M_NTFSNTVATTR */ \
	"NTFS resident data ",	/* 134 M_NTFSRDATA */ \
	"NTFS decomp",	/* 135 M_NTFSDECOMP */ \
	"NTFS vrun",	/* 136 M_NTFSRUN */ \
	"kqueue",	/* 137 M_KEVENT */ \
	"bluetooth",	/* 138 M_BLUETOOTH */ \
	"bwmeter",	/* 139 M_BWMETER */ \
}

struct kmemstats {
	long	ks_inuse;	/* # of packets of this type currently in use */
	long	ks_calls;	/* total packets of this type ever allocated */
	long 	ks_memuse;	/* total memory held in bytes */
	u_short	ks_limblocks;	/* number of times blocked for hitting limit */
	u_short	ks_mapblocks;	/* number of times blocked for kernel map */
	long	ks_maxused;	/* maximum number ever used */
	long	ks_limit;	/* most that are allowed to exist */
	long	ks_size;	/* sizes of this thing that are allocated */
	long	ks_spare;
};

/*
 * Array of descriptors that describe the contents of each page
 */
struct kmemusage {
	short ku_indx;		/* bucket index */
	union {
		u_short freecnt;/* for small allocations, free pieces in page */
		u_short pagecnt;/* for large allocations, pages alloced */
	} ku_un;
};
#define	ku_freecnt ku_un.freecnt
#define	ku_pagecnt ku_un.pagecnt

/*
 * Set of buckets for each size of memory block that is retained
 */
struct kmembuckets {
	caddr_t   kb_next;	/* list of free blocks */
	caddr_t   kb_last;	/* last free block */
	u_int64_t kb_calls;	/* total calls to allocate this size */
	u_int64_t kb_total;	/* total number of blocks allocated */
	u_int64_t kb_totalfree;	/* # of free elements in this bucket */
	u_int64_t kb_elmpercl;	/* # of elements in this sized allocation */
	u_int64_t kb_highwat;	/* high water mark */
	u_int64_t kb_couldfree;	/* over high water mark and could free */
};

#ifdef _KERNEL
#define	MINALLOCSIZE	(1 << MINBUCKET)
#define	BUCKETINDX(size) \
	((size) <= (MINALLOCSIZE * 128) \
		? (size) <= (MINALLOCSIZE * 8) \
			? (size) <= (MINALLOCSIZE * 2) \
				? (size) <= (MINALLOCSIZE * 1) \
					? (MINBUCKET + 0) \
					: (MINBUCKET + 1) \
				: (size) <= (MINALLOCSIZE * 4) \
					? (MINBUCKET + 2) \
					: (MINBUCKET + 3) \
			: (size) <= (MINALLOCSIZE* 32) \
				? (size) <= (MINALLOCSIZE * 16) \
					? (MINBUCKET + 4) \
					: (MINBUCKET + 5) \
				: (size) <= (MINALLOCSIZE * 64) \
					? (MINBUCKET + 6) \
					: (MINBUCKET + 7) \
		: (size) <= (MINALLOCSIZE * 2048) \
			? (size) <= (MINALLOCSIZE * 512) \
				? (size) <= (MINALLOCSIZE * 256) \
					? (MINBUCKET + 8) \
					: (MINBUCKET + 9) \
				: (size) <= (MINALLOCSIZE * 1024) \
					? (MINBUCKET + 10) \
					: (MINBUCKET + 11) \
			: (size) <= (MINALLOCSIZE * 8192) \
				? (size) <= (MINALLOCSIZE * 4096) \
					? (MINBUCKET + 12) \
					: (MINBUCKET + 13) \
				: (size) <= (MINALLOCSIZE * 16384) \
					? (MINBUCKET + 14) \
					: (MINBUCKET + 15))

/*
 * Turn virtual addresses into kmem map indicies
 */
#define	kmemxtob(alloc)	(kmembase + (alloc) * NBPG)
#define	btokmemx(addr)	(((caddr_t)(addr) - kmembase) / NBPG)
#define	btokup(addr)	(&kmemusage[((caddr_t)(addr) - kmembase) >> PAGE_SHIFT])

/*
 * Macro versions for the usual cases of malloc/free
 */
#if defined(KMEMSTATS) || defined(DIAGNOSTIC) || defined(_LKM) || defined(SMALL_KERNEL)
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), type, flags)
#define	FREE(addr, type) free((caddr_t)(addr), type)

#else /* do not collect statistics */
#define	MALLOC(space, cast, size, type, flags) do { \
	register struct kmembuckets *kbp = &bucket[BUCKETINDX(size)]; \
	long __s = splvm(); \
	if (kbp->kb_next == NULL) { \
		(space) = (cast)malloc((u_long)(size), type, flags); \
	} else { \
		(space) = (cast)kbp->kb_next; \
		kbp->kb_next = *(caddr_t *)(space); \
	} \
	splx(__s); \
} while (0)

#define	FREE(addr, type) do { \
	register struct kmembuckets *kbp; \
	register struct kmemusage *kup = btokup(addr); \
	long __s = splvm(); \
	if (1 << kup->ku_indx > MAXALLOCSAVE) { \
		free((caddr_t)(addr), type); \
	} else { \
		kbp = &bucket[kup->ku_indx]; \
		if (kbp->kb_next == NULL) \
			kbp->kb_next = (caddr_t)(addr); \
		else \
			*(caddr_t *)(kbp->kb_last) = (caddr_t)(addr); \
		*(caddr_t *)(addr) = NULL; \
		kbp->kb_last = (caddr_t)(addr); \
	} \
	splx(__s); \
} while(0)
#endif /* do not collect statistics */

extern struct kmemstats kmemstats[];
extern struct kmemusage *kmemusage;
extern char *kmembase;
extern struct kmembuckets bucket[];

extern void *malloc(unsigned long size, int type, int flags);
extern void free(void *addr, int type);
extern int sysctl_malloc(int *, u_int, void *, size_t *, void *, size_t,
			      struct proc *);

size_t malloc_roundup(size_t);

#ifdef MALLOC_DEBUG
int	debug_malloc(unsigned long, int, int, void **);
int	debug_free(void *, int);
void	debug_malloc_init(void);
void	debug_malloc_assert_allocated(void *, const char *);
#define DEBUG_MALLOC_ASSERT_ALLOCATED(addr) 			\
	debug_malloc_assert_allocated(addr, __func__)

void	debug_malloc_print(void);
void	debug_malloc_printit(int (*)(const char *, ...), vaddr_t);
#endif /* MALLOC_DEBUG */
#endif /* _KERNEL */
#endif /* !_SYS_MALLOC_H_ */

/*	$NetBSD: tc.h,v 1.1 1995/02/13 23:09:07 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * TurboChannel-specific functions and structures.
 */

/*
 * A junk address to read from, to make sure writes are complete.  See
 * System Programmer's Manual, section 9.3 (p. 9-4), and sacrifice a
 * chicken.
 */
#define	MAGIC_READ do {							\
	extern u_int32_t no_optimize;					\
	no_optimize = *(u_int32_t *)phystok0seg(0x00000001f0080220);	\
} while (0);

#define	TC_SPACE_IND		0xffffffffe0000003
#define	TC_SPACE_DENSE		0x0000000000000000
#define TC_SPACE_DENSE_OFFSET	0x0000000007fffffc
#define	TC_SPACE_SPARSE		0x0000000010000000
#define	TC_SPACE_SPARSE_OFFSET	0x000000000ffffff8

#define	TC_DENSE_TO_SPARSE(addr)					\
	    ((void *)							\
		(((u_int64_t)addr & TC_SPACE_IND) |			\
		 TC_SPACE_SPARSE |					\
		 (((u_int64_t)addr & TC_SPACE_DENSE_OFFSET) << 1)))
		
#define	TC_SPARSE_TO_DENSE(addr)					\
	    ((void *)							\
		(((u_int64_t)addr & TC_SPACE_IND) |			\
		 TC_SPACE_DENSE |					\
		 (((u_int64_t)addr & TC_SPACE_SPARSE_OFFSET) >> 1)))
		

#define	TC_ROM_LLEN		8
#define	TC_ROM_SLEN		4
#define	TC_ROM_TEST_SIZE	16

#define	TC_SLOT_ROM		0x000003e0
#define	TC_SLOT_PROTOROM	0x003c03e0

typedef struct tc_padchar {
	u_int8_t	v;
	u_int8_t	pad[3];
} tc_padchar_t;

struct tc_rommap {
	tc_padchar_t	tcr_width;
	tc_padchar_t	tcr_stride;
	tc_padchar_t	tcr_rsize;
	tc_padchar_t	tcr_ssize;
	u_int8_t	tcr_test[TC_ROM_TEST_SIZE];
	tc_padchar_t	tcr_rev[TC_ROM_LLEN];
	tc_padchar_t	tcr_vendname[TC_ROM_LLEN];
	tc_padchar_t	tcr_modname[TC_ROM_LLEN];
	tc_padchar_t	tcr_firmtype[TC_ROM_SLEN];
};

/* The contents of a cfdata->cf_loc for a TurboChannel device */
struct tc_cfloc {
	int	cf_slot;		/* Slot number */
	int	cf_offset;		/* XXX? Offset into slot. */
	int	cf_vec;
	int	cf_ipl;
};

#define	TC_SLOT_WILD	-1		/* wildcarded slot */
#define	TC_OFFSET_WILD	-1		/* wildcarded offset */
#define TC_VEC_WILD	-1		/* wildcarded vec */
#define TC_IPL_WILD	-1		/* wildcarded ipl */

struct tc_slot_desc {
	caddr_t	tsd_dense;
};

struct tc_cpu_desc {
	struct	tc_slot_desc *tcd_slots;
	long	tcd_nslots;
	struct	confargs *tcd_devs;
	long	tcd_ndevs;
	void	(*tc_intr_setup) __P((void));
	void	(*tc_intr_establish)
		    __P((struct confargs *, intr_handler_t, void *));
	void	(*tc_intr_disestablish) __P((struct confargs *));
	void	(*tc_iointr) __P((void *, int));
};

int	tc_intrnull __P((void *));

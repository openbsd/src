/*	$NetBSD: wsconsvar.h,v 1.2 1996/04/12 06:10:36 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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

#ifndef _ALPHA_WSCONS_WSCONSVAR_H_
#define _ALPHA_WSCONS_WSCONSVAR_H_

struct device;

typedef int (*wscons_ioctl_t)	__P((struct device *dev, u_long cmd,
				    caddr_t data, int flag, struct proc *p));
typedef int (*wscons_mmap_t)	__P((struct device *dev, off_t off,
				    int prot));

struct wscons_emulfuncs {
	void	(*wef_cursor) __P((void *c, int on, int row, int col));
	void	(*wef_putstr) __P((void *c, int row, int col, char *cp,
		    int n));

	void	(*wef_copycols) __P((void *c, int row, int srccol, int dstcol,
		    int ncols));
	void	(*wef_erasecols) __P((void *c, int row, int startcol,
		    int ncols));

	void	(*wef_copyrows) __P((void *c, int srcrow, int dstrow,
		    int nrows));
	void	(*wef_eraserows) __P((void *c, int row, int nrows));
};

struct wscons_odev_spec {
	const struct wscons_emulfuncs *wo_ef;	/* emulation functions */
	void *wo_efa;				/* emulation function cookie */

	int	wo_nrows, wo_ncols;		/* number of rows & cols */
	int	wo_crow, wo_ccol;		/* current row & col */

	wscons_ioctl_t wo_ioctl;
	wscons_mmap_t wo_mmap;
};

struct wsconsio_bell_data;

struct wscons_idev_spec {
	int	(*wi_getc) __P((struct device *c));
	void	(*wi_pollc) __P((struct device *c, int on));
	void	(*wi_bell) __P((struct device *c, struct wsconsio_bell_data *));
	wscons_ioctl_t wi_ioctl;
	char	*(*wi_translate) __P((struct device *c, int code));
	int	wi_keymask;		/* keyboard code mask */
	int	wi_keyupmask;		/* key went up (vs. down) */
};

struct wscons_mdev_spec {
	int	(*wm_enable) __P((struct device *));
	int	(*wm_disable) __P((struct device *));
};

struct wscons_attach_args {			/* attaches output device */
	int	waa_isconsole;			/* is it the console unit? */
	struct wscons_odev_spec waa_odev_spec;	/* mostly ignored if cons. */
};

#define	wsconscf_console	cf_loc[0]	/* spec'd to be console? */

/*
 * Attach the console output device.  This is called _very_ early
 * on in the boot process.
 */
void	wscons_attach_console __P((const struct wscons_odev_spec *));

/*
 * Attach the console input device.  At this point, it's assumed
 * that there can be only one.  This happens after the input device
 * has been probed.  (XXX boot -d won't work...)
 */
void	wscons_attach_input __P((struct device *,
	    const struct wscons_idev_spec *));

/*
 * Transfer a string of characters from the console input device to
 * the wscons buffer.  (XXX raw scancodes?  pass ioctl, or something?
 * then need length.)
 */
void	wscons_input __P((char *));

#endif /* _ALPHA_WSCONS_WSCONSVAR_H_ */

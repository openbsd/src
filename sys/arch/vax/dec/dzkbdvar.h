/*	$OpenBSD: dzkbdvar.h,v 1.4 2008/08/20 16:31:41 miod Exp $	*/
/* $NetBSD: dzkbdvar.h,v 1.2 2001/03/06 07:40:52 matt Exp $ */

struct dzkm_attach_args {
	int	daa_line;	/* Line to search */
	int	daa_flags;	/* Console etc...*/
#define	DZKBD_CONSOLE	1
};

/* dzcons.c */
int	dz_can_have_kbd(void);
void	dzcninit_internal(int, int);
int	dzcngetc_internal(int);

/* dzinput.c */
void	dzputc(struct dz_linestate *, int);
int	dz_print(void *, const char *);

/* dzkbd.c */
int	dzkbd_cnattach(void);

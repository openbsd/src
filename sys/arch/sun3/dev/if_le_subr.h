/*	$NetBSD: if_le_subr.h,v 1.7 1995/01/03 15:43:40 gwr Exp $	*/

/* One might also set: LE_ACON | LE_BCON */
#define	LE_CONF3 (LE_BSWP)

extern int  le_md_match(struct device *, void *, void *args);
extern void le_md_attach(struct device *, struct device *, void *);
extern int  leintr(void *);

/* $OpenBSD: pckbdvar.h,v 1.1 2000/11/13 20:12:35 aaron Exp $ */
/* $NetBSD: pckbdvar.h,v 1.3 2000/03/10 06:10:35 thorpej Exp $ */

int	pckbd_cnattach __P((pckbc_tag_t, int));
void	pckbd_hookup_bell __P((void (*fn)(void *, u_int, u_int, u_int, int),
	    void *));

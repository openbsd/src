/*	$OpenBSD: autoconf.h,v 1.1 1998/07/07 21:32:38 mickey Exp $	*/


struct pdc_tod;

void	configure	__P((void));
void	dumpconf	__P((void));
void	pdc_iodc __P((int (*)__P((void)), int, ...));


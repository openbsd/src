#	$OpenBSD: Makefile.inc,v 1.2 1997/09/21 11:50:43 deraadt Exp $
#
#	etc.vax/Makefile.inc -- vax-specific etc Makefile targets
#

.ifdef DESTDIR
distribution: pcs750.bin

pcs750.bin:
	(cd etc.vax;uudecode pcs750.bin.uu)

.endif

#	$OpenBSD: Makefile.inc,v 1.1 1996/03/28 18:24:40 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.1 1996/03/14 17:32:16 ragge Exp $
#
#	etc.vax/Makefile.inc -- vax-specific etc Makefile targets
#

.ifdef DESTDIR
distribution: pcs750.bin

pcs750.bin:
	(cd etc.vax;uudecode pcs750.bin.uu)

.endif

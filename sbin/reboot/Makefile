#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:32:05 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/08/08 20:46:21 gwr Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8 boot_hp300.8 boot_i386.8 boot_mac68k.8 boot_pmax.8 \
	boot_sparc.8 boot_sun3.8 boot_tahoe.8 boot_vax.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.include <bsd.prog.mk>

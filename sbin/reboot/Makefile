#	$OpenBSD: Makefile,v 1.3 1996/07/27 10:25:36 deraadt Exp $
#	$NetBSD: Makefile,v 1.10.6.2 1996/06/28 17:28:07 jtc Exp $

PROG=	reboot
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	reboot.8 boot_atari.8 boot_hp300.8 boot_i386.8 boot_mac68k.8 \
	boot_pmax.8 boot_sparc.8 boot_sun3.8 boot_tahoe.8 boot_vax.8 \
	boot_x68k.8
MLINKS=	reboot.8 halt.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt

.include <bsd.prog.mk>

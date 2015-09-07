#	$OpenBSD: Makefile,v 1.1 2015/09/07 01:38:32 yasuoka Exp $

.include <bsd.own.mk>

PROG=		BOOTIA32.EFI
NOMAN=		#
OBJFMT=		efi-app-ia32
SOFTRAID=	yes
#SRCS+=		run_i386.S
SRCS+=		random_i386.S
LDSCRIPT=	ldscript.i386

COPTS=		-I${EFIDIR}/include/i386

.include "${.CURDIR}/../Makefile.common"

CFLAGS+=	-m32
AFLAGS+=	-m32

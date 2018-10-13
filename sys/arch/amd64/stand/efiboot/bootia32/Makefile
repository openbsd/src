#	$OpenBSD: Makefile,v 1.3 2018/10/13 19:56:56 kettenis Exp $

.include <bsd.own.mk>

PROG=		BOOTIA32.EFI
NOMAN=		#
OBJFMT=		efi-app-ia32
SOFTRAID=	yes
SRCS+=		start_i386.S random_i386.S
LDSCRIPT=	ldscript.i386

COPTS=		-I${EFIDIR}/include/i386

.include "${.CURDIR}/../Makefile.common"

CFLAGS+=	-m32
AFLAGS+=	-m32
LDFLAGS+=	-N

#	$OpenBSD: Makefile,v 1.1 2019/05/11 02:33:34 mlarkin Exp $

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

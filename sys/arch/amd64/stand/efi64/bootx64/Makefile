#	$OpenBSD: Makefile,v 1.1 2019/05/11 02:36:10 mlarkin Exp $

.include <bsd.own.mk>

PROG=		BOOTX64.EFI
NOMAN=		#
OBJFMT=		efi-app-x86_64
SOFTRAID=	yes
COPTS+=		-I${EFIDIR}/include/amd64
SRCS+=		start_amd64.S run_i386.S random_amd64.S eficall.S
LDSCRIPT=	ldscript.amd64

.include "${.CURDIR}/../Makefile.common"


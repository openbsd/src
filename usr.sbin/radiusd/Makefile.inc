#	$OpenBSD: Makefile.inc,v 1.2 2017/02/21 05:17:48 yasuoka Exp $
.PATH:		${.CURDIR}/..
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wextra -Wshadow -Wno-unused-parameter
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations -Wpointer-arith

#DEGUG=		-g
#CFLAGS+=	-DRADIUSD_DEBUG
#CFLAGS+=	-Werror

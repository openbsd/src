#	$OpenBSD: Makefile.inc,v 1.1 2015/07/21 04:06:04 yasuoka Exp $
.PATH:		${.CURDIR}/..
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall -Wextra -Wshadow -Wno-unused-parameter
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations -Wpointer-arith
.ifdef DEBUG
CFLAGS+=	-DRADIUSD_DEBUG
CFLAGS+=	-Werror
.endif

#	$OpenBSD: Makefile,v 1.1 1997/03/12 10:42:01 downsj Exp $

LIB=	resolv
SRCS=	base64.c gethnamaddr.c getnetbyaddr.c getnetbyname.c getnetent.c \
	getnetnamadr.c herror.c hostnamelen.c inet_addr.c inet_net_ntop.c \
	inet_net_pton.c inet_neta.c inet_ntop.c inet_pton.c nsap_addr.c \
	res_comp.c res_data.c res_debug.c res_init.c res_mkquery.c \
	res_query.c res_send.c sethostent.c

CFLAGS+=${INCLUDE} ${CONFIG}

# we don't build the other copies of this library
NOPIC=yes
NOPROFILE=yes

# we don't install this version of the library
install:

.include <bsd.lib.mk>
.include "../Makefile.inc"

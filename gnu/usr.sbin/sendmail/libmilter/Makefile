#	$OpenBSD: Makefile,v 1.5 2003/02/19 06:18:10 millert Exp $

LIB=		milter
SRCS=		main.c engine.c listener.c handler.c comm.c smfi.c signal.c \
		sm_gethost.c errstring.c strl.c
CPPFLAGS+=	-D_FFR_MILTER_ROOT_UNSAFE -pthread -Dsm_snprintf=snprintf
.PATH:		${.CURDIR}/../libsm

# This is not a library that gets installed so only build the .a version
# In the future we may wish to install it to ease the use of external filters.
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>

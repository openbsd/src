#	$OpenBSD: Makefile,v 1.3 2017/07/29 12:45:56 bluhm Exp $

PROG=	bltone
LDADD+=	-L/usr/X11R6/lib -L/usr/X11R6/lib/modules -lfb
LDADD+=	-Wl,-rpath /usr/X11R6/lib/modules
LDADD+=	-Wl,--allow-shlib-undefined

.include <bsd.regress.mk>

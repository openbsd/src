#	$OpenBSD: Makefile,v 1.2 2001/01/28 19:34:31 niklas Exp $

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c
MAN =	rpc.rusersd.8
MLINKS=	rpc.rusersd.8 rusersd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.if exists(/usr/X11)
XINCDIR=	/usr/X11/include
XLIBDIR=	/usr/X11/lib
.elif exists(/usr/X11R6)
XINCDIR=	/usr/X11R6/include
XLIBDIR=	/usr/X11R6/lib
.elif exists(/usr/X11R5)
XINCDIR=	/usr/X11R5/include
XLIBDIR=	/usr/X11R5/lib
.elif exists(/usr/X11R4)
XINCDIR=	/usr/X11R4/include
XLIBDIR=	/usr/X11R4/lib
.elif exists(/usr/X386)
XINCDIR=	/usr/X386/include
XLIBDIR=	/usr/X386/lib
.endif

.if defined(XINCDIR) && exists($(XINCDIR)/X11/extensions/xidle.h)
CFLAGS+=	-I$(XINCDIR) -DXIDLE
LDADD+=		-L$(XLIBDIR) -lXext -lX11
.endif


.include <bsd.prog.mk>


#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:40:59 deraadt Exp $

PROG=	ci
CFLAGS+= -I${.CURDIR}/../lib
LDADD=	${LIBRCS} -lgnumalloc
DPADD=	${LIBRCS} /usr/lib/libgnumalloc.a

.include "../../Makefile.inc"
.include <bsd.prog.mk>

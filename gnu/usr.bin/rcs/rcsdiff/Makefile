#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:41:03 deraadt Exp $

PROG=	rcsdiff
CFLAGS+= -I${.CURDIR}/../lib
LDADD=	${LIBRCS} -lgnumalloc
DPADD=	${LIBRCS} /usr/lib/libgnumalloc.a

.include "../../Makefile.inc"
.include <bsd.prog.mk>

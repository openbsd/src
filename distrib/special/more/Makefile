#	$OpenBSD: Makefile,v 1.11 2015/11/14 23:40:12 deraadt Exp $

PROG=	more
CFLAGS+= -I. -I${.CURDIR}/
SRCS=	more.c tputs.c termcap.c tgoto.c 

CLEANFILES+=morehelp.h

beforedepend: morehelp.h

morehelp.h: ${.CURDIR}/more.help
	echo -n 'const char *more_help = "' > morehelp.h
	sed -e 's/[\\"]/\\&/g' -e 's/$$/\\n\\/' ${.CURDIR}/more.help >> morehelp.h
	echo '";' >> morehelp.h

more.o: morehelp.h

.include <bsd.prog.mk>

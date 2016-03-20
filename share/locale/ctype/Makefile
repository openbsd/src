# $OpenBSD: Makefile,v 1.11 2016/03/20 15:45:40 jca Exp $
# $NetBSD: Makefile,v 1.20 2005/02/10 18:03:02 tnozaki Exp $

NOMAN=	# defined

# pull LOCALEDIR and other declarations
.include <bsd.own.mk>

all: UTF-8.out

UTF-8.out: en_US.UTF-8.src
	${CPP} < ${.CURDIR}/en_US.UTF-8.src | \
	    sed -e '/^#/d' | mklocale -o ${.TARGET}

CLEANFILES+=	UTF-8.out

install:
	install -o ${LOCALEOWN} -g ${LOCALEGRP} -m ${DIRMODE} -d \
	    ${DESTDIR}${LOCALEDIR}/UTF-8
	install -o ${LOCALEOWN} -g ${LOCALEGRP} -m ${LOCALEMODE} \
	    UTF-8.out ${DESTDIR}${LOCALEDIR}/UTF-8/LC_CTYPE

.include <bsd.prog.mk>

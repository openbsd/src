#	$NetBSD: Makefile,v 1.4 1995/04/20 22:43:03 cgd Exp $

PROG=	doexec
NOMAN=	noman, no way, man

LDSTATIC= -static

RP=	${.OBJDIR}/${PROG}
TD=	${.CURDIR}/tests
OD=	${.CURDIR}/good

all:	${PROG} goodaout truncaout
CLEANFILES+= goodaout truncaout

install:

regress:	test-empty test-nonexist \
		test-nonexistshell test-devnullscript test-badinterplen \
		test-goodscript test-scriptarg test-scriptarg-nospace \
		test-goodaout test-truncaout

test-empty:	${PROG} ${TD}/empty
	${RP} ${TD}/empty | diff - ${OD}/empty

test-nonexist:	${PROG}
	${RP} ${TD}/nonexistent | diff - ${OD}/nonexistent

test-nonexistshell: ${PROG} ${TD}/nonexistshell
	${RP} ${TD}/nonexistshell | diff - ${OD}/nonexistshell

test-devnullscript: ${PROG} ${TD}/devnullscript
	${RP} ${TD}/devnullscript | diff - ${OD}/devnullscript

test-badinterplen: ${PROG} ${TD}/badinterplen
	${RP} ${TD}/badinterplen | diff - ${OD}/badinterplen

test-goodscript: ${PROG} ${TD}/goodscript
	${RP} ${TD}/goodscript | diff - ${OD}/goodscript

test-scriptarg: ${PROG} ${TD}/scriptarg
	${RP} ${TD}/scriptarg 2>&1 | diff - ${OD}/scriptarg

test-scriptarg-nospace: ${PROG} ${TD}/scriptarg-nospace
	${RP} ${TD}/scriptarg-nospace 2>&1 | diff - ${OD}/scriptarg-nospace

goodaout: ${TD}/goodaout.c
	${LINK.c} ${LDSTATIC} -o ${.TARGET} ${TD}/goodaout.c ${LDLIBS}

test-goodaout: ${PROG} goodaout
	${RP} ${.OBJDIR}/goodaout | diff - ${OD}/goodaout

truncaout: goodaout
	/bin/rm -rf truncaout
	dd if=${.OBJDIR}/goodaout of=truncaout bs=16 count=1
	chmod a+x truncaout

test-truncaout: ${PROG} truncaout
	${RP} ${.OBJDIR}/truncaout | diff - ${OD}/truncaout

.include <bsd.prog.mk>

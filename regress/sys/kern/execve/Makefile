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

test-empty:	${PROG} ${TD}/empty.in
	@cp ${TD}/empty.in ${.OBJDIR}/empty
	@chmod 555 ${.OBJDIR}/empty
	${RP} ${.OBJDIR}/empty | diff - ${OD}/empty

test-nonexist:	${PROG}
	${RP} ${TD}/nonexistent | diff - ${OD}/nonexistent

test-nonexistshell: ${PROG} ${TD}/nonexistshell.in
	@cp ${TD}/nonexistshell.in ${.OBJDIR}/nonexistshell
	@chmod 555 ${.OBJDIR}/nonexistshell
	${RP} ${.OBJDIR}/nonexistshell | diff - ${OD}/nonexistshell

test-devnullscript: ${PROG} ${TD}/devnullscript.in
	@cp ${TD}/devnullscript.in ${.OBJDIR}/devnullscript
	@chmod 555 ${.OBJDIR}/devnullscript
	${RP} ${.OBJDIR}/devnullscript | diff - ${OD}/devnullscript

test-badinterplen: ${PROG} ${TD}/badinterplen.in
	@cp ${TD}/badinterplen.in ${.OBJDIR}/badinterplen
	@chmod 555 ${.OBJDIR}/badinterplen
	${RP} ${.OBJDIR}/badinterplen | diff - ${OD}/badinterplen

test-goodscript: ${PROG} ${TD}/goodscript.in
	@cp ${TD}/goodscript.in ${.OBJDIR}/goodscript
	@chmod 555 ${.OBJDIR}/goodscript
	${RP} ${.OBJDIR}/goodscript | diff - ${OD}/goodscript

test-scriptarg: ${PROG} ${TD}/scriptarg.in
	@cp ${TD}/scriptarg.in ${.OBJDIR}/scriptarg
	@chmod 555 ${.OBJDIR}/scriptarg
	${RP} ${.OBJDIR}/scriptarg 2>&1 | diff - ${OD}/scriptarg

test-scriptarg-nospace: ${PROG} ${TD}/scriptarg-nospace.in
	@cp ${TD}/scriptarg-nospace.in ${.OBJDIR}/scriptarg-nospace
	@chmod 555 ${.OBJDIR}/scriptarg-nospace
	${RP} ${.OBJDIR}/scriptarg-nospace 2>&1 | diff - ${OD}/scriptarg-nospace

goodaout: ${TD}/goodaout.c
	${LINK.c} ${LDSTATIC} -o ${.TARGET} ${TD}/goodaout.c ${LDLIBS}

test-goodaout: ${PROG} goodaout
	${RP} ${.OBJDIR}/goodaout | diff - ${OD}/goodaout

truncaout: goodaout
	/bin/rm -rf truncaout
	dd if=${.OBJDIR}/goodaout of=truncaout bs=16 count=1
	@chmod a+x truncaout

test-truncaout: ${PROG} truncaout
	${RP} ${.OBJDIR}/truncaout | diff - ${OD}/truncaout

.include <bsd.prog.mk>

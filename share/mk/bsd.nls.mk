#	$OpenBSD: bsd.nls.mk,v 1.5 1997/04/27 21:38:31 millert Exp $
#	$NetBSD: bsd.nls.mk,v 1.2 1995/04/27 18:05:38 jtc Exp $

.if !target(.MAIN)
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.MAIN: all
.endif

.SUFFIXES: .cat .msg

.msg.cat:
	@rm -f ${.TARGET}
	gencat ${.TARGET} ${.IMPSRC}

.if defined(NLS) && !empty(NLS)
NLSALL= ${NLS:.msg=.cat}
.endif

.if !defined(NLSNAME)
.if defined(PROG)
NLSNAME=${PROG}
.else
NLSNAME=lib${LIB}
.endif
.endif

nlsinstall:
.if defined(NLSALL)
	@for msg in ${NLSALL}; do \
		NLSLANG=`basename $$msg .cat`; \
		dir=${DESTDIR}${NLSDIR}/$${NLSLANG}; \
		${INSTALL} -d $$dir; \
		${INSTALL} ${INSTALL_COPY} -o ${NLSOWN} -g ${NLSGRP} -m ${NLSMODE} $$msg $$dir/${NLSNAME}.cat; \
	done
.endif

.if defined(NLSALL)
all: ${NLSALL}

install:  nlsinstall

cleandir: cleannls
cleannls:
	rm -f ${NLSALL}
.endif

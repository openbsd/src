#	$OpenBSD: Makefile,v 1.2 2004/02/01 14:38:47 jmc Exp $
#	@(#)Makefile	8.1 (Berkeley) 8/14/93

DIR=	psd/15.yacc
SRCS=	ss.. ss0 ss1 ss2 ss3 ss4 ss5 ss6 ss7 ss8 ss9 ssA ssB ssa ssb ssc ssd
MACROS=	-ms
REFER=	refer -e # -p /usr/share/dict/papers/Ind

paper.ps: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} > ${.TARGET}

paper.txt: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} -Tascii > ${.TARGET}

.include <bsd.doc.mk>

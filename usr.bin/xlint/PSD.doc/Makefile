#	OpenBSD$

DIR=	psd/09.lint
SRCS=	lint.ms
MACROS=	-ms
REFER=	refer -e -p /usr/doc/run/Ind

paper.${PRINTER}: ${SRCS}
	${REFER} ${SRCS} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>

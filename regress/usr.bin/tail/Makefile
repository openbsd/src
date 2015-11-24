# $OpenBSD: Makefile,v 1.2 2015/11/24 04:04:19 tedu Exp $

REGRESS_TARGETS = follow-renames follow-overwrite-by-file follow-overwrite-data follow-removed-recreated follow-newsyslog
REGRESS_TARGETS+= forward-blocks forward-chars forward-lines reverse-blocks reverse-chars reverse-lines
REGRESS_TARGETS+= forward-blocks-rprint forward-chars-rprint forward-lines-rprint
REGRESS_TARGETS+= reverse-blocks-rprint reverse-chars-rprint reverse-lines-rprint
REGRESS_ROOT_TARGETS = follow-newsyslog

.for t in ${REGRESS_ROOT_TARGETS}
${t}:
	${SUDO} sh ${.CURDIR}/${t}.sh
.endfor

.for t in ${REGRESS_TARGETS}
${t}:
	sh ${.CURDIR}/${t}.sh
.endfor

.include <bsd.regress.mk>

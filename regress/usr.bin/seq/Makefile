#	$OpenBSD: Makefile,v 1.2 2023/06/13 04:59:10 anton Exp $

SEQ?=	/usr/bin/seq

CLEANFILES+=	*.out

REGRESS_TARGETS+=seqtest
seqtest:
	sh ${.CURDIR}/$@.sh ${SEQ} $@.out
	diff ${.CURDIR}/$@.expected $@.out

.include <bsd.regress.mk>

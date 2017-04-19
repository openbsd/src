#	$OpenBSD: Makefile,v 1.2 2017/04/19 12:44:05 bluhm Exp $

REGRESS_TARGETS=	hit miss
REGRESS_ROOT_TARGETS=	hit miss

.ifmake !obj && !clean && !cleandir && !depend && !regress
.if (${.TARGET} != all && ! make(all)) || (${.TARGET} == all)

.SILENT: .BEGIN .END .INTERRUPT

.BEGIN:
	${SUDO} pfctl -qt __regress_tbl -T add -f ${.CURDIR}/table.in

.END:
	${SUDO} pfctl -qt __regress_tbl -T kill

.INTERRUPT:
	${SUDO} pfctl -qt __regress_tbl -T kill

.endif
.endif

hit:
	for i in `cat ${.CURDIR}/table.hit`; do \
		echo -n "hit $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i || exit 1; \
		echo success; \
	done; \
	exit 0

miss:
	for i in `cat ${.CURDIR}/table.miss`; do \
		echo -n "miss $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i && exit 1; \
		echo success; \
	done; \
	exit 0

.PHONY: hit miss

.include <bsd.regress.mk>

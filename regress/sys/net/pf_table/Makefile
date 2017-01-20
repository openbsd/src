#	$OpenBSD: Makefile,v 1.1 2017/01/20 04:54:44 claudio Exp $

REGRESS_TARGETS=	hit miss
REGRESS_ROOT_TARGETS=	hit miss

.ifmake !obj && !clean && !cleandir && !depend && !regress
.if (${.TARGET} != all && ! make(all)) || (${.TARGET} == all)

.SILENT: .BEGIN .END .INTERRUPT

.BEGIN:
	${SUDO} pfctl -qt __regress_tbl -T add -f ./table.in

.END:
	${SUDO} pfctl -qt __regress_tbl -T kill

.INTERRUPT:
	${SUDO} pfctl -qt __regress_tbl -T kill

.endif
.endif

hit:
	for i in `cat ./table.hit`; do \
		echo -n "hit $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i || exit 1; \
		echo success; \
	done; \
	exit 0

miss:
	for i in `cat ./table.miss`; do \
		echo -n "miss $$i "; \
		${SUDO} pfctl -qt __regress_tbl -T test $$i && exit 1; \
		echo success; \
	done; \
	exit 0

.PHONY: hit miss

.include <bsd.regress.mk>

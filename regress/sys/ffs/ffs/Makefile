# $OpenBSD: Makefile,v 1.4 2017/11/15 14:43:13 bluhm Exp $

TESTS=		chflags chmod chown link mkdir mkfifo open rename rmdir \
		symlink truncate unlink
CLEANFILES=	diskimage stamp-*

.PHONY: disk mount unconfig clean

disk: unconfig
	dd if=/dev/zero of=diskimage bs=512 count=4k
	vnconfig vnd0 diskimage
	newfs vnd0c

mount: disk
	mkdir -p /mnt/regress-ffs
	mount /dev/vnd0c /mnt/regress-ffs

unconfig:
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir /mnt/regress-ffs 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-rm -f stamp-setup

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} mount
	date >$@

${.OBJDIR}/../fstest:
	${.MAKE} -C ${.CURDIR}/.. fstest

.for t in ${TESTS}
REGRESS_TARGETS+=	run-regress-${t}
run-regress-${t}: stamp-setup ${.OBJDIR}/../fstest
	@echo '\n======== $@ ========'
	cd /mnt/regress-ffs && env FSTEST=${.OBJDIR}/../fstest \
	    /bin/sh ${.CURDIR}/../run ${.CURDIR}/../tests/${t}/*.t
.endfor

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	umount /mnt/regress-ffs
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

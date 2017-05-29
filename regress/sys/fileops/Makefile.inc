# $OpenBSD: Makefile.inc,v 1.2 2017/05/29 13:49:40 bluhm Exp $

TESTS=		create read mmap

FILEOPS_MNT=	/mnt/regress-fileops
FILEOPS_PROG=	${.OBJDIR}/../fileops
CLEANFILES=	diskimage stamp-*

.poison !defined (MOUNT)
.poison !defined (NEWFS)

.PHONY: disk mount unconfig clean

disk: unconfig
	dd if=/dev/urandom of=diskimage bs=1M count=32
	vnconfig vnd0 diskimage
	${NEWFS} /dev/rvnd0c

mount: disk
	mkdir -p ${FILEOPS_MNT}
	${MOUNT} /dev/vnd0c ${FILEOPS_MNT}

unconfig:
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir ${FILEOPS_MNT} 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-rm -f stamp-setup diskimage

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} mount
	date >$@

${.OBJDIR}/../fileops:
	${.MAKE} -C ${.CURDIR}/.. fileops

.for t in ${TESTS}
REGRESS_TARGETS+=	run-regress-${t}
run-regress-${t}: stamp-setup ${.OBJDIR}/../fileops
	@echo '\n======== $@ ========'
	cd ${FILEOPS_MNT} && \
	    ${FILEOPS_PROG} ${t} ${FILEOPS_MNT}/file
.endfor

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	umount ${FILEOPS_MNT}
	${.MAKE} -C ${.CURDIR} unconfig

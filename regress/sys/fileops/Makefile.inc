# $OpenBSD: Makefile.inc,v 1.4 2020/12/16 21:49:20 bluhm Exp $

PERL_REQUIRE != perl -e 'eval { require File::Slurp } or print $$@'

TESTS1=		create read mmap
TESTS2=		many_files_root many_files_subdir file_write
TESTS= ${TESTS1} ${TESTS2}

FILEOPS_MNT=	/mnt/regress-fileops
FILEOPS_PROG=	${.OBJDIR}/../fileops
CLEANFILES=	diskimage

.poison !defined (MOUNT)
.poison !defined (NEWFS)

.PHONY: disk mount unconfig clean

disk: unconfig
	dd if=/dev/urandom of=diskimage bs=1M count=64
	vnconfig vnd0 diskimage
	${NEWFS} /dev/rvnd0c

REGRESS_SETUP_ONCE+=	mount
mount: disk
	mkdir -p ${FILEOPS_MNT}
	${MOUNT} /dev/vnd0c ${FILEOPS_MNT}

REGRESS_CLEANUP+=	umount
umount:
	umount ${FILEOPS_MNT}

REGRESS_CLEANUP+=	unconfig
unconfig:
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir ${FILEOPS_MNT} 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	rm -f stamp-setup

${.OBJDIR}/../fileops:
	${.MAKE} -C ${.CURDIR}/.. fileops

.for t in ${TESTS1}
REGRESS_TARGETS+=	run-$t
run-$t: ${.OBJDIR}/../fileops
	cd ${FILEOPS_MNT} && \
	    ${FILEOPS_PROG} $t ${FILEOPS_MNT}/file
.endfor

.for t in ${TESTS2}
REGRESS_TARGETS+=	run-$t
run-$t: ${.CURDIR}/../fileops2.pl
.if ! empty(PERL_REQUIRE)
	@echo "${PERL_REQUIRE}"
	@echo "Install the p5-File-Slurp package to run fileops $t subtest"
	@echo SKIPPED
.else
	perl ${.CURDIR}/../fileops2.pl $t ${FILEOPS_MNT}
.endif
.endfor

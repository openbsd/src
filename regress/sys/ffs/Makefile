# $OpenBSD: Makefile,v 1.4 2011/04/18 16:52:11 thib Exp $

PROG=	fstest

mount:
	dd if=/dev/zero of=${.CURDIR}/fakeobj bs=512 count=4k
	vnconfig vnd0 ${.CURDIR}/fakeobj
	newfs /dev/rvnd0c
	mount /dev/vnd0c /mnt

clean:
	-umount /mnt
	-vnconfig -u vnd0
	-vnconfig -u vnd1
	-rm -f ${.CURDIR}/fakeobj
	-rm -f ${.OBJDIR}/fstest
	-rm -f ${.OBJDIR}/fstest.o

chflags: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/chflags/*.t

chmod: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/chmod/*.t

chown: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/chown/*.t
	
link: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/link/*.t

mkdir: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/mkdir/*.t

mkfifo: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/mkfifo/*.t

open: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/open/*.t

rename: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/rename/*.t

rmdir: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/rmdir/*.t

symlink: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/symlink/*.t

truncate: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/truncate/*.t

unlink: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/unlink/*.t

run-regress-fstest: mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest /bin/sh ${.CURDIR}/run\
		${.CURDIR}/tests/*/*.t

.include <bsd.regress.mk>

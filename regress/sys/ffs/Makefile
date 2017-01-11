# $OpenBSD: Makefile,v 1.6 2017/01/11 00:02:48 bluhm Exp $

PROG=	fstest

mount: unmount
	dd if=/dev/zero of=fakeobj bs=512 count=4k
	vnconfig vnd0 fakeobj
	newfs /dev/rvnd0c
	mount /dev/vnd0c /mnt

unmount:
	-umount -f /mnt 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-vnconfig -u vnd1 2>/dev/null || true

clean: unmount
	-rm -f fakeobj
	-rm -f fstest
	-rm -f fstest.o

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
	cd ${.CURDIR} && ${.MAKE} unmount

.include <bsd.regress.mk>

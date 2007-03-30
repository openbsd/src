# $OpenBSD: Makefile,v 1.1 2007/03/30 19:02:51 pedro Exp $

PROG=	fstest

mount:
	dd if=/dev/zero of=${.CURDIR}/fakeobj bs=512 count=4k
	vnconfig svnd0 ${.CURDIR}/fakeobj
	newfs /dev/rsvnd0c
	mount /dev/svnd0c /mnt

clean:
	-umount -f /mnt
	-vnconfig -u svnd0
	-vnconfig -u svnd1
	-rm -f ${.CURDIR}/fakeobj
	-rm -f ${.OBJDIR}/fstest
	-rm -f ${.OBJDIR}/fstest.o

chflags: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/chflags/*.t

chmod: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/chmod/*.t

chown: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/chown/*.t
	
link: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/link/*.t

mkdir: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/mkdir/*.t

mkfifo: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/mkfifo/*.t

open: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/open/*.t

rename: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/rename/*.t

rmdir: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/rmdir/*.t

symlink: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/symlink/*.t

truncate: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/truncate/*.t

unlink: ${PROG} mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/unlink/*.t

run-regress-fstest: mount
	cd /mnt && env FSTEST=${.OBJDIR}/fstest ${.CURDIR}/run\
		${.CURDIR}/tests/*/*.t

.include <bsd.regress.mk>

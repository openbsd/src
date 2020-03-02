# $OpenBSD: Makefile,v 1.2 2020/03/02 12:03:09 bluhm Exp $

# Call realpath(3) in combination with chroot(2), chdir(2).
# Use umount(8) to check that the mountpoint leaks no vnode.
# There were vnode reference counting bugs in the kernel.

PROGS=		realpath-chroot realpath-chdir
CLEANFILES=	diskimage

.PHONY: mount unconfig clean

diskimage: unconfig
	@echo '\n======== $@ ========'
	${SUDO} dd if=/dev/zero of=diskimage bs=512 count=4k
	${SUDO} vnconfig vnd0 diskimage
	${SUDO} newfs vnd0c

mount: diskimage
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath
	${SUDO} mount /dev/vnd0c /mnt/regress-realpath

unconfig:
	@echo '\n======== $@ ========'
	-${SUDO} umount -f /dev/vnd0c 2>/dev/null || true
	-${SUDO} rmdir /mnt/regress-realpath 2>/dev/null || true
	-${SUDO} vnconfig -u vnd0 2>/dev/null || true
	-${SUDO} rm -f stamp-setup

REGRESS_SETUP	=	${PROGS} mount
REGRESS_CLEANUP =	unconfig
REGRESS_TARGETS =

REGRESS_TARGETS +=	run-chroot
run-chroot:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath
	${SUDO} ./realpath-chroot /mnt/regress-realpath /
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chroot-dir
run-chroot-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo
	${SUDO} ./realpath-chroot /mnt/regress-realpath/foo /
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chroot-subdir
run-chroot-subdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo
	${SUDO} ./realpath-chroot /mnt/regress-realpath /foo
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chroot-dir-subdir
run-chroot-dir-subdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo/bar
	${SUDO} ./realpath-chroot /mnt/regress-realpath/foo /bar
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chdir
run-chdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath
	${SUDO} ./realpath-chdir /mnt/regress-realpath .
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chdir-dir
run-chdir-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo
	${SUDO} ./realpath-chdir /mnt/regress-realpath/foo .
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chdir-subdir
run-chdir-subdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo
	${SUDO} ./realpath-chdir /mnt/regress-realpath foo
	${SUDO} umount /mnt/regress-realpath

REGRESS_TARGETS +=	run-chdir-dir-subdir
run-chdir-dir-subdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-realpath/foo/bar
	${SUDO} ./realpath-chdir /mnt/regress-realpath/foo bar
	${SUDO} umount /mnt/regress-realpath

REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS}

.include <bsd.regress.mk>

# $OpenBSD: Makefile,v 1.1.1.1 2019/08/01 15:20:51 bluhm Exp $

# Call unveil(2) in combination with unlink(2) and chroot(2).
# Use umount(8) to check that the mountpoint leaks no vnode.
# There were vnode reference counting bugs in the kernel.

PROGS=		unveil-unlink unveil-chroot unveil-perm
CLEANFILES=	diskimage

.PHONY: mount unconfig clean

diskimage: unconfig
	${SUDO} dd if=/dev/zero of=diskimage bs=512 count=4k
	${SUDO} vnconfig vnd0 diskimage
	${SUDO} newfs vnd0c

mount: diskimage
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} mount /dev/vnd0c /mnt/regress-unveil

unconfig:
	@echo '\n======== $@ ========'
	-${SUDO} umount -f /dev/vnd0c 2>/dev/null || true
	-${SUDO} rmdir /mnt/regress-unveil 2>/dev/null || true
	-${SUDO} vnconfig -u vnd0 2>/dev/null || true
	-${SUDO} rm -f stamp-setup

REGRESS_SETUP	=	${PROGS} mount
REGRESS_CLEANUP =	unconfig
REGRESS_TARGETS =

REGRESS_TARGETS +=	run-unlink
run-unlink:
	@echo '\n======== $@ ========'
	# unlink a file in an unveiled directory
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-unlink /mnt/regress-unveil/foo bar
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot
run-chroot:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-chroot /mnt/regress-unveil /
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir
run-chroot-dir:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-dir
run-chroot-unveil-dir:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chroot /mnt/regress-unveil /foo
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-unveil-dir
run-chroot-dir-unveil-dir:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /bar
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-open
run-chroot-open:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil / /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-open
run-chroot-dir-open:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo / /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-dir-open
run-chroot-unveil-dir-open:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil /foo /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-unveil-dir-open
run-chroot-dir-unveil-dir-open:
	@echo '\n======== $@ ========'
	# unveil in a chroot environment
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} touch /mnt/regress-unveil/foo/bar/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /bar /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm
run-perm:
	@echo '\n======== $@ ========'
	# unveil in a perm environment
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-perm "" /mnt/regress-unveil
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir
run-perm-dir:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-perm "" /mnt/regress-unveil/foo
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-open
run-perm-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-open
run-perm-dir-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-create-open
run-perm-create-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "c" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-create-open
run-perm-dir-create-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "c" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-write-open
run-perm-write-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "w" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-write-open
run-perm-dir-write-open:
	@echo '\n======== $@ ========'
	# unveil with permission
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "w" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS}

.include <bsd.regress.mk>

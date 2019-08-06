# $OpenBSD: Makefile,v 1.4 2019/08/06 11:38:16 bluhm Exp $

# Call unveil(2) in combination with unlink(2), chroot(2), chdir(2).
# Use umount(8) to check that the mountpoint leaks no vnode.
# There were vnode reference counting bugs in the kernel.

PROGS=		unveil-unlink unveil-chroot unveil-perm unveil-chdir
CLEANFILES=	diskimage

.PHONY: mount unconfig clean

diskimage: unconfig
	@echo '\n======== $@ ========'
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
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-unlink /mnt/regress-unveil/foo bar
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot
run-chroot:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-chroot /mnt/regress-unveil /
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir
run-chroot-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-dir
run-chroot-unveil-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chroot /mnt/regress-unveil /foo
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-unveil-dir
run-chroot-dir-unveil-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /bar
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-rootslash
run-chroot-rootslash:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-chroot /mnt/regress-unveil //
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-open
run-chroot-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil / /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-open
run-chroot-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo / /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-dir-open
run-chroot-unveil-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil /foo /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-dir-unveil-dir-open
run-chroot-dir-unveil-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} touch /mnt/regress-unveil/foo/bar/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil/foo /bar /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-rootslash-open
run-chroot-rootslash-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil // /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-open-rootslash
run-chroot-open-rootslash:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil / //baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-rootdot-open
run-chroot-unveil-rootdotdot-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil /. /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chroot-unveil-rootdotdot-open
run-chroot-unveil-rootdotdot-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chroot /mnt/regress-unveil /.. /baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm
run-perm:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-perm "" /mnt/regress-unveil
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir
run-perm-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-perm "" /mnt/regress-unveil/foo
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-open
run-perm-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-open
run-perm-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-create-open
run-perm-create-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "c" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-create-open
run-perm-dir-create-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "c" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-write-open
run-perm-write-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-perm "w" /mnt/regress-unveil baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-perm-dir-write-open
run-perm-dir-write-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-perm "w" /mnt/regress-unveil/foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir
run-chdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} ./unveil-chdir /mnt/regress-unveil .
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir
run-chdir-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo .
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-dir
run-chdir-unveil-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chdir /mnt/regress-unveil foo
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-backdir
run-chdir-unveil-backdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chdir /mnt/regress-unveil foo/..
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-dotdot
run-chdir-unveil-dotdot:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo ..
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-dir
run-chdir-dir-unveil-dir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo bar
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-backdir
run-chdir-dir-unveil-backdir:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo bar/..
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-dotdot
run-chdir-dir-unveil-dotdot:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo/bar ..
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-open
run-chdir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil . baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-open
run-chdir-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo . baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-dir-open
run-chdir-unveil-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil foo baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-backdir-open
run-chdir-unveil-backdir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil foo/.. baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-unveil-dotdot-open
run-chdir-unveil-dotdot-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo
	${SUDO} touch /mnt/regress-unveil/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo .. baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-dir-open
run-chdir-dir-unveil-dir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} touch /mnt/regress-unveil/foo/bar/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo bar baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-backdir-open
run-chdir-dir-unveil-backdir-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo bar/.. baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_TARGETS +=	run-chdir-dir-unveil-dotdot-open
run-chdir-dir-unveil-dotdot-open:
	@echo '\n======== $@ ========'
	${SUDO} mkdir -p /mnt/regress-unveil/foo/bar
	${SUDO} touch /mnt/regress-unveil/foo/baz
	${SUDO} ./unveil-chdir /mnt/regress-unveil/foo/bar .. baz
	${SUDO} umount /mnt/regress-unveil

REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS}

.include <bsd.regress.mk>

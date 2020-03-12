#	$OpenBSD: Makefile,v 1.1 2020/03/12 15:40:39 visa Exp $

REGRESS_SETUP=		setup
REGRESS_CLEANUP=	cleanup
REGRESS_TARGETS=

setup:
	mkdir -p /mnt/regress-vnd

cleanup: unconfig
	-rmdir /mnt/regress-vnd 2>/dev/null || true

unconfig:
	-vnconfig -u vnd0 2>/dev/null || true
	-umount -f /mnt/regress-vnd 2>/dev/null || true

REGRESS_TARGETS+=	run-regress-async-mount
run-regress-async-mount: unconfig
	mount_mfs -s1M -oasync swap /mnt/regress-vnd
	dd if=/dev/zero of=/mnt/regress-vnd/disk.img count=1024
	vnconfig vnd0 /mnt/regress-vnd/disk.img
	echo 'e 0\nC\n\n64\n960\nw\nq\n' | fdisk -e vnd0
	newfs -t msdos vnd0i

REGRESS_ROOT_TARGETS=	${REGRESS_TARGETS}

.include <bsd.regress.mk>

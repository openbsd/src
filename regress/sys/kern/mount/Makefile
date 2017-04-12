# $OpenBSD: Makefile,v 1.4 2017/04/12 14:46:31 bluhm Exp $

# This regress test uses a disk template to create 15 partitions on
# a vnd device.  All are initialized with a ffs file system.  Then
# they are mounted nestedly in /mnt, see disktemplate for the tree
# layout.  The unmount-nested test uses umount -f to unmount /mnt,
# without -f it does not work.  It is checked that the kernel does
# the unmount recursively.  There must be no dangling mount points.
# The whole tests have to be run as root.

CLEANFILES=	diskimage stamp-*

.PHONY: disk mount unconfig clean

disk: unconfig
	dd if=/dev/zero of=diskimage bs=100k count=16
	vnconfig vnd0 diskimage
	disklabel -wA -T ${.CURDIR}/disktemplate vnd0
	disklabel vnd0
.for p in a b d e f g h i j k l m n o p
	newfs vnd0${p}
.endfor
	disklabel vnd0 | grep -q '16 partitions:'
	[ `disklabel vnd0 | grep -c '\<4.2BSD\>'` -eq 15 ]

mount: disk
	mkdir -p /mnt/regress-mount
	mount /dev/vnd0a /mnt/regress-mount
	mkdir /mnt/regress-mount/b
	mount /dev/vnd0b /mnt/regress-mount/b
	f=/mnt/regress-mount; for p in d e f g h i j k l m n o p;\
	    do f=$$f/$$p; mkdir $$f; mount /dev/vnd0$$p $$f; done
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 15 ]

unconfig:
	-umount -f /dev/vnd0a 2>/dev/null || true
	-rmdir /mnt/regress-mount 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-rm -f stamp-setup

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} mount
	date >$@

REGRESS_TARGETS+=	run-regress-unmount-nested
run-regress-unmount-nested: stamp-setup
	@echo '\n======== $@ ========'
	! umount /mnt/regress-mount
	umount -f /mnt/regress-mount
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 0 ]

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

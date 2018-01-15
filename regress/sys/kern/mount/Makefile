# $OpenBSD: Makefile,v 1.5 2018/01/15 23:58:43 bluhm Exp $

# This regress test uses a vnd device to run mount and unmount.
# All tests have to be run as root.

CLEANFILES=	diskimage *.log

.PHONY: disk mount unconfig clean

disk: unconfig
	dd if=/dev/null of=diskimage bs=1m seek=1100
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

mount-nested: mount
	mkdir /mnt/regress-mount/b
	mount /dev/vnd0b /mnt/regress-mount/b
	f=/mnt/regress-mount; for p in d e f g h i j k l m n o p;\
	    do f=$$f/$$p; mkdir $$f; mount /dev/vnd0$$p $$f; done
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 15 ]

unconfig:
	@echo '\n======== $@ ========'
	-umount -f /dev/vnd0a 2>/dev/null || true
	-rmdir /mnt/regress-mount 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true

# The unmount-nested test uses a disk template to create 15 partitions
# on a vnd device.  All are initialized with a ffs file system.
# Then they are mounted nestedly in /mnt, see disktemplate for the
# tree layout.  The unmount-nested test uses umount -f to unmount
# /mnt, without -f it does not work.  It is checked that the kernel
# does the unmount recursively.  There must be no dangling mount
# points.

REGRESS_TARGETS+=	run-regress-unmount-nested
run-regress-unmount-nested: mount-nested
	@echo '\n======== $@ ========'
	! umount /mnt/regress-mount
	umount -f /mnt/regress-mount
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 0 ]

# Create a 1 GB vnd partition and fill the ffs file system it with
# cp -r.  After 15 seconds clean it with rm -rf.  While this is
# running, unmount with -f.  Run fsck -f to see that everything is
# clean.

REGRESS_TARGETS+=	run-regress-unmount-busy
run-regress-unmount-busy: mount
	@echo '\n======== $@ ========'
	cp -r /usr /mnt/regress-mount &
	sleep 5
	sync
	sleep 10
	rm -rf /mnt/regress-mount/usr &
	sleep .1
	umount -f /mnt/regress-mount
	fsck -f -y /dev/rvnd0a 2>&1 | tee fsck.log
	! egrep 'yes|FILE SYSTEM WAS MODIFIED|MARKING FILE SYSTEM CLEAN' \
	    fsck.log

# Create a 1 GB vnd partition and fill the ffs file system it with
# cp -r.  After 15 seconds clean it with rm -rf.  While this is
# running, remount read-only with -ur.  Unmount the partition and
# run fsck -f to see that everything is clean.

REGRESS_TARGETS+=	run-regress-mount-readonly
run-regress-mount-readonly: mount
	@echo '\n======== $@ ========'
	cp -r /usr /mnt/regress-mount &
	sleep 5
	sync
	sleep 10
	rm -rf /mnt/regress-mount/usr &
	sleep .1
	mount -f -ur /mnt/regress-mount
	# XXX mount -ur should not return before it is done
	for i in `jot 20`; do \
	    pgrep -xf 'cp -r /usr' || \
	    pgrep -xf 'rm -rf /mnt/regress-mount/usr' || \
	    break; sleep 1; done
	umount /mnt/regress-mount
	fsck -f -y /dev/rvnd0a 2>&1 | tee fsck.log
	! egrep 'yes|FILE SYSTEM WAS MODIFIED|MARKING FILE SYSTEM CLEAN' \
	    fsck.log

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

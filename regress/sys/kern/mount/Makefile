# $OpenBSD: Makefile,v 1.7 2020/05/25 11:08:12 bluhm Exp $

# This regress test uses a vnd device to run mount and unmount.
# All tests have to be run as root.

CLEANFILES=	diskimage *.log

.PHONY: disk mount unconfig clean

disk: unconfig
	@echo '======== $@ ========'
	dd if=/dev/null of=diskimage bs=1m seek=1100
	vnconfig vnd0 diskimage
	disklabel -wA -T ${.CURDIR}/disktemplate vnd0
	disklabel vnd0
	disklabel vnd0 | grep -q '16 partitions:'
	[ `disklabel vnd0 | grep -c '\<4.2BSD\>'` -eq 15 ]

mount: disk
	@echo '======== $@ ========'
	newfs vnd0a
	mkdir -p /mnt/regress-mount
	mount /dev/vnd0a /mnt/regress-mount

mount-nested: mount
	@echo '======== $@ ========'
.for p in b d e f g h i j k l m n o p
	newfs -O 1 vnd0${p}
.endfor
	mkdir /mnt/regress-mount/b
	mount /dev/vnd0b /mnt/regress-mount/b
	f=/mnt/regress-mount; for p in d e f g h i j k l m n o p;\
	    do f=$$f/$$p; mkdir $$f; mount /dev/vnd0$$p $$f; done
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 15 ]

unconfig:
	@echo '======== $@ ========'
	-umount -f /dev/vnd0a 2>/dev/null || true
	-rmdir /mnt/regress-mount 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true

REGRESS_CLEANUP =	unconfig

# The unmount-nested test uses a disk template to create 15 partitions
# on a vnd device.  All are initialized with a ffs file system.
# Then they are mounted nestedly in /mnt, see disktemplate for the
# tree layout.  The unmount-nested test uses umount -f to unmount
# /mnt, without -f it does not work.  It is checked that the kernel
# does the unmount recursively.  There must be no dangling mount
# points.

REGRESS_TARGETS+=	run-unmount-nested
run-unmount-nested: mount-nested
	@echo '\n======== $@ ========'
	! umount /mnt/regress-mount
	umount -f /mnt/regress-mount
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt/regress-mount\>'` -eq 0 ]

# Create a 1 GB vnd partition and fill the ffs file system it with
# cp -r.  After 15 seconds clean it with rm -rf.  While this is
# running, unmount with -f.  Run fsck -f to see that everything is
# clean.

REGRESS_TARGETS+=	run-unmount-busy
run-unmount-busy: mount
	@echo '\n======== $@ ========'
	cp -r /usr /mnt/regress-mount &
	sleep 5
	sync
	sleep 10
	rm -rf /mnt/regress-mount/usr &
	sleep .1
	umount -f /mnt/regress-mount
	fsck -y /dev/rvnd0a 2>&1 | tee fsck-clean.log
	fsck -f -y /dev/rvnd0a 2>&1 | tee fsck-force.log
	egrep -q 'File system is clean' \
	    fsck-clean.log
	! egrep -q 'yes|FILE SYSTEM WAS MODIFIED|MARKING FILE SYSTEM CLEAN' \
	    fsck-force.log

run-readonly-busy run-readonly-dangling:
	# Remounting a file system read-only that has dangling vnodes fails
	# currently.  It is marked as clean but has unreferneced files.
	@echo DISABLED

# Create a 1 GB vnd partition and fill the ffs file system it with
# cp -r.  After 15 seconds clean it with rm -rf.  While this is
# running, remount read-only with -ur.  Unmount the partition and
# run fsck -f to see that a file system marked as clean is really
# clean.

REGRESS_TARGETS+=	run-readonly-busy
run-readonly-busy: mount
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
	fsck -y /dev/rvnd0a 2>&1 | tee fsck-clean.log
	fsck -f -y /dev/rvnd0a 2>&1 | tee fsck-force.log
	! egrep -q 'File system is clean' fsck-clean.log || \
	! egrep -q 'yes|FILE SYSTEM WAS MODIFIED|MARKING FILE SYSTEM CLEAN' \
	    fsck-force.log

# Open a file, unlink it, and remount the file system read-only.
# Then the file system cannot be clean after unmounting it.
# Check that clean flag is not set and repair it with fsck -y.

REGRESS_TARGETS+=	run-readonly-dangling
run-readonly-dangling: mount
	@echo '\n======== $@ ========'
	touch /mnt/regress-mount/file
	sleep 73 </mnt/regress-mount/file &
	sleep .1
	rm /mnt/regress-mount/file
	mount -ur /mnt/regress-mount
	pkill -xf 'sleep 73'
	sleep .1
	umount /mnt/regress-mount
	fsck -y /dev/rvnd0a 2>&1 | tee fsck-clean.log
	fsck -f -y /dev/rvnd0a 2>&1 | tee fsck-force.log
	! egrep -q 'File system is clean' fsck-clean.log
	egrep -q 'yes|FILE SYSTEM WAS MODIFIED|MARKING FILE SYSTEM CLEAN' \
	    fsck-clean.log fsck-force.log

.include <bsd.regress.mk>

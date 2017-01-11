# $OpenBSD: Makefile,v 1.1.1.1 2017/01/11 17:10:32 bluhm Exp $

.PHONY: disk mount unconfig clean

disk: clean
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
	mount /dev/vnd0a /mnt
	mkdir /mnt/b
	mount /dev/vnd0b /mnt/b
	f=/mnt; for p in d e f g h i j k l m n o p;\
	    do f=$$f/$$p; mkdir $$f; mount /dev/vnd0$$p $$f; done
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt\>'` -eq 15 ]

unconfig:
	-umount -f /mnt 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true

clean: unconfig
	-rm -f diskimage

REGRESS_TARGETS=	run-regress-unmount-nested
run-regress-unmount-nested: mount
	! umount /mnt
	umount -f /mnt
	mount
	[ `mount | grep -c '/dev/vnd0. on /mnt\>'` -eq 0 ]
	cd ${.CURDIR} && ${.MAKE} unconfig

.include <bsd.regress.mk>

# $OpenBSD: Makefile,v 1.7 2017/09/04 12:04:03 bluhm Exp $

PROGS=		mmap-sysctl-copyin mmap-sysctl-copyout
CLEANFILES=	diskimage stamp-*

.PHONY: disk nfs mount unconfig clean

disk: unconfig
	dd if=/dev/zero of=diskimage bs=512 count=4k
	vnconfig vnd0 diskimage
	newfs vnd0c

nfs:
	grep '/mnt/regress-nfs-server\>' /etc/exports || \
	    echo /mnt/regress-nfs-server -maproot=0:0 127.0.0.1 >>/etc/exports
	rcctl -f start portmap
	rcctl -f start nfsd
	rcctl -f start mountd

mount: disk nfs
	mkdir -p /mnt/regress-nfs-server
	mount /dev/vnd0c /mnt/regress-nfs-server
	# wait until mountd(8) has exported the directory
	for i in `jot 100`; do \
	    mount | grep 'regress-nfs-server .*NFS exported' && break; \
	    [ $$i = 100 ] && exit 1; \
	    sleep .1; \
	done
	mkdir -p /mnt/regress-nfs-client
	mount -t nfs 127.0.0.1:/mnt/regress-nfs-server /mnt/regress-nfs-client

unconfig:
	-umount -f -t nfs -h 127.0.0.1 -a || true
	-rmdir /mnt/regress-nfs-client 2>/dev/null || true
	-pkill -KILL mountd || true
	-rcctl -f stop nfsd
	-rcctl -f stop portmap
	-umount -f /dev/vnd0c 2>/dev/null || true
	-rmdir /mnt/regress-nfs-server 2>/dev/null || true
	-vnconfig -u vnd0 2>/dev/null || true
	-rm -f stamp-setup

stamp-setup:
	@echo '\n======== $@ ========'
	${.MAKE} -C ${.CURDIR} mount
	date >$@

REGRESS_TARGETS+=	run-regress-read
run-regress-read: stamp-setup
	@echo '\n======== $@ ========'
	echo -n $@ >/mnt/regress-nfs-server/read
	[ $@ = "`cat /mnt/regress-nfs-client/read`" ]

REGRESS_TARGETS+=	run-regress-write
run-regress-write: stamp-setup
	@echo '\n======== $@ ========'
	echo -n $@ >/mnt/regress-nfs-client/write
	[ $@ = "`cat /mnt/regress-nfs-server/write`" ]

.for p in ${PROGS}
REGRESS_TARGETS+=	run-regress-${p}
run-regress-${p}: stamp-setup ${p}
	@echo '\n======== $@ ========'
	./${p}
.endfor

.for socktype nctype in stream -U dgram -Uu
REGRESS_TARGETS+=	run-regress-socket-${socktype}
run-regress-socket-${socktype}: stamp-setup
	@echo '\n======== $@ ========'
	rm -f /mnt/regress-nfs-client/socket-${socktype}
	nc ${nctype} -v -l /mnt/regress-nfs-client/socket-${socktype} &
	[ -S /mnt/regress-nfs-client/socket-${socktype} ] || sleep 1
	[ -S /mnt/regress-nfs-client/socket-${socktype} ]
	nc ${nctype} -z /mnt/regress-nfs-client/socket-${socktype}
.if "${socktype}" == dgram
	pkill -xf "nc -Uu -v -l /mnt/regress-nfs-client/socket-dgram"
.endif
.endfor

REGRESS_TARGETS+=	run-regress-cleanup
run-regress-cleanup:
	@echo '\n======== $@ ========'
	-pkill -xf "nc -U -v -l /mnt/regress-nfs-client/socket-stream" || true
	-pkill -xf "nc -Uu -v -l /mnt/regress-nfs-client/socket-dgram" || true
	${.MAKE} -C ${.CURDIR} unconfig

.include <bsd.regress.mk>

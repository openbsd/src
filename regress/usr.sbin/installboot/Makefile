#	$OpenBSD: Makefile,v 1.12 2022/08/31 23:10:01 kn Exp $

INSTALLBOOT ?=		/usr/sbin/installboot
DRY_RUN =		${INSTALLBOOT} -n
REAL_RUN =		${INSTALLBOOT}

# installboot(8) behaviour for multi-chunk softraid(4) differs across platforms
NDISKS ?=		1 2
DISKFILES =		${NDISKS:=disk%.img}
DISKDEVFILES =		${NDISKS:=diskdev%.txt}
ROOTDEVFILE =		rootdev.txt
MOUNTPOINT =		mnt
STAGEDIR =		/usr/mdec

.if   ${MACHINE} == "amd64" ||	\
      ${MACHINE} == "i386"	# assume BIOS/MBR
FORMAT_DISK =		fdisk -iy
STAGENAMES =		biosboot boot
.elif ${MACHINE} == "arm64"
FORMAT_DISK =		fdisk -g -y -b32768
STAGENAMES =		BOOTAA64.EFI
.elif ${MACHINE} == "sparc64"
FORMAT_DISK =		true	# NOOP
STAGENAMES =		bootblk ofwboot
.endif
STAGEFILES =		${STAGENAMES:=${STAGEDIR}/%}

REGRESS_SETUP_ONCE =	copy-bootstrap-to-root

create-disks:
.for n in ${NDISKS}
	dd if=/dev/zero  of=disk${n}.img bs=1m count=0 seek=64 status=none
	${SUDO} vnconfig -- disk${n}.img 1>diskdev${n}.txt
.endfor

format-disks: create-disks
.for devfile in ${DISKDEVFILES}
	${SUDO} ${FORMAT_DISK} -- "$$(<${devfile})" 1>/dev/null
	printf 'a\n\n\n\nRAID\nw\nq\n' | \
		${SUDO} disklabel -E -- "$$(<${devfile})" 1>/dev/null
.endfor

create-root: format-disks
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DISKDEVFILES} | paste -sd, -- -)" \
	    -cc -- softraid0 | \
		awk -- '{ print $$NF }' 1>${ROOTDEVFILE}

create-mountpoint:
	mkdir -- ${MOUNTPOINT}

format-root: create-root create-mountpoint
	${SUDO} ${FORMAT_DISK} -- "$$(<${ROOTDEVFILE})" 1>/dev/null
	${SUDO} disklabel -Aw -- "$$(<${ROOTDEVFILE})"
	${SUDO} newfs -q -- "$$(<${ROOTDEVFILE})"a 1>/dev/null
	${SUDO} mount -- /dev/"$$(<${ROOTDEVFILE})"a ${MOUNTPOINT}

copy-bootstrap-to-root: format-root
	${SUDO} mkdir -- ${MOUNTPOINT}/usr
	${SUDO} cp -r -- ${STAGEDIR} ${MOUNTPOINT}/usr/


# must be first as some targets expect a working filesystem
REGRESS_TARGETS = prepare

prepare:
	${SUDO} ${REAL_RUN} -p -- "$$(<${ROOTDEVFILE})"
	# XXX -p is not yet softraid(4) aware, need to prepare chunks manually
.for devfile in ${DISKDEVFILES}
	${SUDO} ${REAL_RUN} -p -- "$$(<${devfile})"
.endfor

REGRESS_TARGETS +=	dry-prepare \
			dry-default \
			dry-root \
			root \
			root-explicit-stages

dry-prepare:
	${SUDO} ${DRY_RUN} -p -- "$$(<${ROOTDEVFILE})"

dry-default:
	${SUDO} ${DRY_RUN} -- "$$(<${ROOTDEVFILE})"

dry-root:
	${SUDO} ${DRY_RUN} -r/ -- "$$(<${ROOTDEVFILE})"

# XXX fails with NDISKS > 1 on sparc64, 1 <= NDISKS <= 4 works on amd64
root:
	${SUDO} ${REAL_RUN} -r ${MOUNTPOINT} "$$(<${ROOTDEVFILE})"

root-explicit-stages:
	${SUDO} ${REAL_RUN} -- "$$(<${ROOTDEVFILE})" ${STAGEFILES}


REGRESS_EXPECTED_FAILURES =	dry-prepare-root \
				dry-prepare-stage \
				dry-nodisk-stage \
				dry-toomany
REGRESS_TARGETS +=		${REGRESS_EXPECTED_FAILURES}

dry-prepare-root:
	${DRY_RUN} -p -r/ -- "$$(<${ROOTDEVFILE})"

dry-prepare-stage:
	${DRY_RUN} -p -- "$$(<${ROOTDEVFILE})" ${STAGEFILES}

dry-nodisk-stage:
	${SUDO} ${DRY_RUN} -- ${STAGEFILES}

dry-toomany:
	${DRY_RUN} -- disk stage1 stage2 too many


CLEANFILES =		${DISKFILES} ${DISKDEVFILES} ${ROOTDEVFILE}
REGRESS_CLEANUP =	cleanup

# allow failure to always cleanup as much as possible
cleanup:
	-${SUDO} umount -- ${MOUNTPOINT} 2>/dev/null
	-rmdir -- ${MOUNTPOINT} 2>/dev/null
	-${SUDO} bioctl -d -- "$$(<${ROOTDEVFILE})" 2>/dev/null
.for devfile in ${DISKDEVFILES}
	-${SUDO} vnconfig -u -- "$$(<${devfile})" 2>/dev/null
.endfor

.include <bsd.regress.mk>

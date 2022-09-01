#	$OpenBSD: Makefile,v 1.14 2022/09/01 00:43:15 kn Exp $

INSTALLBOOT ?=		/usr/sbin/installboot
DRY_RUN =		${INSTALLBOOT} -n
REAL_RUN =		${INSTALLBOOT}


# see distrib/${MACHINE}/ramdisk/install.md for FORMAT_DISK values
# default to root on softraid(4) on all architectures that have
# bootstrap + kernel + installboot(8) support for it
.if   ${MACHINE} == "amd64" ||	\
      ${MACHINE} == "i386"	# assume BIOS/MBR
FORMAT_DISK =		fdisk -iy
STAGENAMES =		biosboot boot
USE_SOFTRAID ?=		Yes
.elif ${MACHINE} == "arm64"
FORMAT_DISK =		fdisk -g -y -b32768
STAGENAMES =		BOOTAA64.EFI
USE_SOFTRAID ?=		Yes
.elif ${MACHINE} == "macppc"	# assume MBR
FORMAT_DISK =		fdisk -iy -b2048@1:06
STAGENAMES =		ofwboot
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "sparc64"
FORMAT_DISK =		true	# NOOP
STAGENAMES =		bootblk ofwboot
USE_SOFTRAID ?=		Yes
.endif

STAGEDIR =		/usr/mdec
STAGEFILES =		${STAGENAMES:=${STAGEDIR}/%}

.if ${USE_SOFTRAID:L} == "yes"
# installboot(8) behaviour for multi-chunk softraid(4) differs across platforms
NDISKS ?=		1 2
.else
NDISKS =		1
.endif
DISKFILES =		${NDISKS:=disk%.img}
DISKDEVFILES =		${NDISKS:=diskdev%.txt}
ROOTDEVFILE =		rootdev.txt
MOUNTPOINT =		mnt


REGRESS_SETUP_ONCE =	copy-bootstrap-to-root

create-disks:
.for n in ${NDISKS}
	dd if=/dev/zero  of=disk${n}.img bs=1m count=0 seek=64 status=none
	${SUDO} vnconfig -- disk${n}.img 1>diskdev${n}.txt
.endfor

format-disks: create-disks
.for devfile in ${DISKDEVFILES}
	${SUDO} ${FORMAT_DISK} -- "$$(<${devfile})" 1>/dev/null
.  if ${USE_SOFTRAID:L} == "yes"
	printf 'a\n\n\n\nRAID\nw\nq\n' | \
		${SUDO} disklabel -E -- "$$(<${devfile})" 1>/dev/null
.  endif
.endfor

create-root: format-disks
.if ${USE_SOFTRAID:L} == "yes"
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DISKDEVFILES} | paste -sd, -- -)" \
	    -cc -- softraid0 | \
		awk -- '{ print $$NF }' 1>${ROOTDEVFILE}
.else
	ln -s -- ${DISKDEVFILES} ${ROOTDEVFILE}
.endif

create-mountpoint:
	mkdir -- ${MOUNTPOINT}

format-root: create-root create-mountpoint
.if ${USE_SOFTRAID:L} != "yes"
	${SUDO} ${FORMAT_DISK} -- "$$(<${ROOTDEVFILE})" 1>/dev/null
.endif
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
.if ${USE_SOFTRAID:L} == "yes"
	# XXX -p is not yet softraid(4) aware, need to prepare chunks manually
.  for devfile in ${DISKDEVFILES}
	${SUDO} ${REAL_RUN} -p -- "$$(<${devfile})"
.  endfor
.endif

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
.if ${USE_SOFTRAID:L} == "yes"
	-${SUDO} bioctl -d -- "$$(<${ROOTDEVFILE})" 2>/dev/null
.endif
.for devfile in ${DISKDEVFILES}
	-${SUDO} vnconfig -u -- "$$(<${devfile})" 2>/dev/null
.endfor

.include <bsd.regress.mk>

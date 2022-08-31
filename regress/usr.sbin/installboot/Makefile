#	$OpenBSD: Makefile,v 1.10 2022/08/31 20:48:55 kn Exp $

INSTALLBOOT ?=		/usr/sbin/installboot
DRY_RUN =		${INSTALLBOOT} -n
REAL_RUN =		${INSTALLBOOT}

# installboot(8) behaviour for multi-chunk softraid(4) differs across platforms
NCHUNKS ?=		1 2
DISKFILES =		${NCHUNKS:=disk%.img}
DISKDEVFILES =		${NCHUNKS:=diskdev%.txt}
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

REGRESS_SETUP_ONCE =	copy-bootstrap-to-softraid

create-new-chunks:
.for n in ${NCHUNKS}
	dd if=/dev/zero  of=disk${n}.img bs=1m count=0 seek=64 status=none
	${SUDO} vnconfig -- disk${n}.img 1>diskdev${n}.txt
.endfor

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
format-new-chunks: create-new-chunks
.for devfile in ${DISKDEVFILES}
	${SUDO} ${FORMAT_DISK} -- "$$(<${devfile})" 1>/dev/null
	printf 'a\n\n\n\nRAID\nw\nq\n' | \
		${SUDO} disklabel -E -- "$$(<${devfile})" 1>/dev/null
.endfor

create-new-softraid: format-new-chunks
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DISKDEVFILES} | paste -sd, -- -)" \
	    -cc -- softraid0 | \
		awk -- '{ print $$NF }' 1>${ROOTDEVFILE}

create-mountpoint:
	mkdir -- ${MOUNTPOINT}

format-new-softraid: create-new-softraid create-mountpoint
	${SUDO} ${FORMAT_DISK} -- "$$(<${ROOTDEVFILE})" 1>/dev/null
	${SUDO} disklabel -Aw -- "$$(<${ROOTDEVFILE})"
	${SUDO} newfs -q -- "$$(<${ROOTDEVFILE})"a 1>/dev/null
	${SUDO} mount -- /dev/"$$(<${ROOTDEVFILE})"a ${MOUNTPOINT}

copy-bootstrap-to-softraid: format-new-softraid
	${SUDO} mkdir -- ${MOUNTPOINT}/usr
	${SUDO} cp -r -- ${STAGEDIR} ${MOUNTPOINT}/usr/


# most but not all usages rquire the EFI filesystem to be usable
.if ${MACHINE} == "arm64"
REGRESS_TARGETS = prepare

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
# do this as regress target and not in format-new-chunks
prepare:
	${SUDO} ${REAL_RUN} -p -- "$$(<${ROOTDEVFILE})"
	# XXX -p is not yet softraid(4) aware, need to prepare chunks manually
.  for devfile in ${DISKDEVFILES}
	${SUDO} ${REAL_RUN} -p -- "$$(<${devfile})"
.  endfor
.else
REGRESS_TARGETS =	# empty
.endif
REGRESS_TARGETS +=	dry-prepare \
			dry-default \
			dry-root \
			root-installer \
			root-explicit-stages

dry-prepare:
	${SUDO} ${DRY_RUN} -p -- "$$(<${ROOTDEVFILE})"

dry-default:
	${SUDO} ${DRY_RUN} -- "$$(<${ROOTDEVFILE})"

dry-root:
	${SUDO} ${DRY_RUN} -r/ -- "$$(<${ROOTDEVFILE})"

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
# XXX fails with NCHUNKS > 1 on sparc64, 1 <= NCHUNKS <= 4 works on amd64
root-installer:
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
	-${SUDO} umount -- ${MOUNTPOINT}
	-rmdir -- ${MOUNTPOINT}
	-${SUDO} bioctl -d -- "$$(<${ROOTDEVFILE})"
.for devfile in ${DISKDEVFILES}
	-${SUDO} vnconfig -u -- "$$(<${devfile})"
.endfor

.include <bsd.regress.mk>

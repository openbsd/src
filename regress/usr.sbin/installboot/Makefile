#	$OpenBSD: Makefile,v 1.4 2022/08/24 15:58:06 kn Exp $

INSTALLBOOT ?=		/usr/sbin/installboot
DRY_RUN =		${INSTALLBOOT} -n
REAL_RUN =		${INSTALLBOOT}

# installboot(8) behaviour for multi-chunk softraid(4) differs across platforms
NCHUNKS ?=		1 2
CHUNKFILES =		${NCHUNKS:=chunk%.img}
DEVFILES =		${NCHUNKS:=vnd%.txt}
SRFILE =		sr.txt
MOUNTPOINT =		mnt
STAGEDIR =		/usr/mdec
.if   ${MACHINE} == "amd64"	# assume BIOS/MBR
STAGENAMES =		biosboot boot
.elif ${MACHINE} == "arm64"
STAGENAMES =		BOOTAA64.EFI
.elif ${MACHINE} == "sparc64"
STAGENAMES =		bootblk ofwboot
.endif
STAGEFILES =		${STAGENAMES:=${STAGEDIR}/%}

REGRESS_SETUP_ONCE =	copy-bootstrap-to-softraid

create-new-chunks:
.for n in ${NCHUNKS}
	dd if=/dev/zero  of=chunk${n}.img bs=1m count=0 seek=32 status=none
	${SUDO} vnconfig -- chunk${n}.img 1>vnd${n}.txt
.endfor

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
format-new-chunks: create-new-chunks
.for devfile in ${DEVFILES}
.if   ${MACHINE} == "amd64"	# assume BIOS/MBR
	${SUDO} fdisk -iy -- "$$(<${devfile})" 1>/dev/null
.elif ${MACHINE} == "arm64"
	${SUDO} fdisk -g -y -b32768 -- "$$(<${devfile})" 1>/dev/null
.endif
	printf 'a\n\n\n\nRAID\nw\nq\n' | \
		${SUDO} disklabel -E -- "$$(<${devfile})" 1>/dev/null
.endfor

create-new-softraid: format-new-chunks
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DEVFILES} | paste -sd, -- -)" \
	    -cc -- softraid0 | \
		awk -- '{ print $$NF }' 1>${SRFILE}

create-mountpoint:
	mkdir -- ${MOUNTPOINT}

format-new-softraid: create-new-softraid create-mountpoint
	${SUDO} disklabel -Aw -- "$$(<${SRFILE})"
	${SUDO} newfs -q -- "$$(<${SRFILE})"a 1>/dev/null
	${SUDO} mount -- /dev/"$$(<${SRFILE})"a ${MOUNTPOINT}

copy-bootstrap-to-softraid: format-new-softraid
	${SUDO} mkdir -- ${MOUNTPOINT}/usr
	${SUDO} cp -r -- ${STAGEDIR} ${MOUNTPOINT}/usr/


# most but not all usages rquire the EFI filesystem to be usable
.if ${MACHINE} == "arm64"
REGRESS_TARGETS = prepare

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
# do this as regress target and not in format-new-chunks
# XXX -p is not yet softraid(4) aware
prepare:
	${SUDO} ${REAL_RUN} -p -- "$$(<${SRFILE})"
.else
REGRESS_TARGETS =	# empty
.endif
REGRESS_TARGETS +=	dry-prepare \
			dry-default \
			dry-root \
			root-installer \
			root-explicit-stages

dry-prepare:
	${SUDO} ${DRY_RUN} -p -- "$$(<${SRFILE})"

dry-default:
	${SUDO} ${DRY_RUN} -- "$$(<${SRFILE})"

dry-root:
	${SUDO} ${DRY_RUN} -r/ -- "$$(<${SRFILE})"

# what the installer does, see /usr/src/distrib/$(machine)/ramdisk/install.md
# XXX fails with N > 1 on sparc64, 1 <= N <= 4 works on amd64
root-installer:
	${SUDO} ${REAL_RUN} -r ${MOUNTPOINT} "$$(<${SRFILE})"

root-explicit-stages:
	${SUDO} ${REAL_RUN} -- "$$(<${SRFILE})" ${STAGEFILES}


REGRESS_EXPECTED_FAILURES =	dry-prepare-root \
				dry-prepare-stage \
				dry-nodisk-stage \
				dry-toomany
REGRESS_TARGETS +=		${REGRESS_EXPECTED_FAILURES}

dry-prepare-root:
	${DRY_RUN} -p -r/ -- "$$(<${SRFILE})"

dry-prepare-stage:
	${DRY_RUN} -p -- "$$(<${SRFILE})" ${STAGEFILES}

dry-nodisk-stage:
	${SUDO} ${DRY_RUN} -- ${STAGEFILES}

dry-toomany:
	${DRY_RUN} -- disk stage1 stage2 too many


CLEANFILES =		${CHUNKFILES} ${DEVFILES} ${SRFILE} ${MOUNTPOINT}
REGRESS_CLEANUP =	cleanup

# allow failure to always cleanup as much as possible
cleanup:
	-${SUDO} umount -- ${MOUNTPOINT}
	- rmdir -- ${MOUNTPOINT}
	-${SUDO} bioctl -d -- "$$(<${SRFILE})"
.for devfile in ${DEVFILES}
	-${SUDO} vnconfig -u -- "$$(<${devfile})"
.endfor

.include <bsd.regress.mk>

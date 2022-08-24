#	$OpenBSD: Makefile,v 1.3 2022/08/24 15:04:18 kn Exp $

INSTALLBOOT ?=		/usr/sbin/installboot
DRY_RUN =		${INSTALLBOOT} -n
REAL_RUN =		${INSTALLBOOT}

# installboot(8) behaviour for multi-chunk softraid(4) differs across platforms
NCHUNKS ?=		1 2
CHUNKFILES =		${NCHUNKS:=chunk%.img}
DEVFILES =		${NCHUNKS:=vnd%.txt}
SRFILE =		sr.txt
# allow testing with real bootstrap, e.g. for size constraints or formats
STAGEFILE ?=		stage.empty
MOUNTPOINT ?=		/mnt

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

format-new-softraid: create-new-softraid
	${SUDO} disklabel -Aw -- "$$(<${SRFILE})"
	${SUDO} newfs -- "$$(<${SRFILE})"a
	${SUDO} mount -- /dev/"$$(<${SRFILE})"a ${MOUNTPOINT}

copy-bootstrap-to-softraid: format-new-softraid
	${SUDO} mkdir -- ${MOUNTPOINT}/usr
	${SUDO} cp -r -- /usr/mdec ${MOUNTPOINT}/usr/


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
	${SUDO} installboot -r ${MOUNTPOINT} "$$(<${SRFILE})"

root-explicit-stages:
.if   ${MACHINE} == "amd64"	# assume BIOS/MBR
	${SUDO} installboot -- "$$(<${SRFILE})" ${MOUNTPOINT}/usr/mdec/biosboot ${MOUNTPOINT}/usr/mdec/boot
.elif ${MACHINE} == "arm64"
	${SUDO} installboot -- "$$(<${SRFILE})" ${MOUNTPOINT}/usr/mdec/BOOTAA64.EFI
.elif ${MACHINE} == "sparc64"
	${SUDO} installboot -- "$$(<${SRFILE})" ${MOUNTPOINT}/usr/mdec/bootblk ${MOUNTPOINT}/usr/mdec/ofwboot
.endif


REGRESS_EXPECTED_FAILURES =	dry-prepare-root \
				dry-prepare-stage \
				dry-nodisk-stage \
				dry-toomany
REGRESS_TARGETS +=		${REGRESS_EXPECTED_FAILURES}

dry-prepare-root:
	${DRY_RUN} -p -r/ -- "$$(<${SRFILE})"

dry-prepare-stage:
	touch -- ${STAGEFILE}
	${DRY_RUN} -p -- "$$(<${SRFILE})" ${STAGEFILE}

dry-nodisk-stage:
	touch -- ${STAGEFILE}
	${SUDO} ${DRY_RUN} -- ${STAGEFILE}

dry-toomany:
	touch -- ${STAGEFILE}
	${DRY_RUN} -- disk stage1 stage2 too many


CLEANFILES =		${CHUNKFILES} ${DEVFILES} ${SRFILE} ${STAGEFILE}
REGRESS_CLEANUP =	cleanup

# allow failure to always cleanup as much as possible
cleanup:
	-${SUDO} umount -- ${MOUNTPOINT}
	-${SUDO} bioctl -d -- "$$(<${SRFILE})"
.for devfile in ${DEVFILES}
	-${SUDO} vnconfig -u -- "$$(<${devfile})"
.endfor

.include <bsd.regress.mk>

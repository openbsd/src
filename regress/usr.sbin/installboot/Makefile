#	$OpenBSD: Makefile,v 1.35 2022/11/08 12:08:53 kn Exp $

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
STAGENAMES =		BOOTAA64.EFI
USE_SOFTRAID ?=		Yes
.elif ${MACHINE} == "armv7"
STAGENAMES =		BOOTARM.EFI
.elif ${MACHINE} == "macppc"	# assume MBR
FORMAT_DISK =		fdisk -iy -b '2048@1:06'
STAGENAMES =		ofwboot
.elif ${MACHINE} == "octeon"
FORMAT_DISK =		fdisk -iy -b '65536@64:C'
STAGENAMES =		boot
.elif ${MACHINE} == "powerpc64"
STAGENAMES =		boot
.elif ${MACHINE} == "riscv64"
STAGENAMES =		BOOTRISCV64.EFI
.elif ${MACHINE} == "sparc64"
FORMAT_DISK =		true	# NOOP
STAGENAMES =		bootblk ofwboot
USE_SOFTRAID ?=		Yes
.else
# missing: alpha, hppa, landisk, loongson, luna88k
regress:
	@echo SKIPPED
.endif

FORMAT_DISK ?=		fdisk -iy -b '32768@32768:C'
STAGEDIR =		/usr/mdec
STAGEFILES =		${STAGENAMES:=${STAGEDIR}/%}
USE_SOFTRAID ?=		No

.if ${USE_SOFTRAID:L} == "yes"
# install on multi-chunk RAID 1C with keydisk so installboot(8) must
# - iterate over chunks
# - ignore the keydisk/not install to it
NDISKS ?=		2
KEYDISKFILE =		keydisk.img
KEYDEVFILE =		keydev.txt
.else
NDISKS =		1
.endif

DISKFILES !=		jot -w disk%d.img    -- ${NDISKS}
DISKDEVFILES !=		jot -w diskdev%d.txt -- ${NDISKS}
ROOTDEVFILE =		rootdev.txt
MOUNTPOINT =		mnt


REGRESS_SETUP_ONCE =	create-root

# |1.img ... n.img|    |1.img 1.txt|
# |1.txt ... n.txt| => |...   ...  |
#                      |n.img n.txt|
FILETUPLES !=		{ echo ${DISKFILES}    ${KEYDISKFILE}; \
			  echo ${DISKDEVFILES} ${KEYDEVFILE} ; } | \
				rs -T -- $$((${NDISKS} + 1)) 2
create-disks:
.for diskfile devfile in ${FILETUPLES}
	dd if=/dev/zero  of=${diskfile} bs=1m count=0 seek=128 status=none
	${SUDO} vnconfig -- ${diskfile} 1>${devfile}
	${SUDO} ${FORMAT_DISK} -- "$$(<${devfile})" 1>/dev/null
.  if ${USE_SOFTRAID:L} == "yes"
	echo 'RAID 1M-* 100%' | \
		${SUDO} disklabel -wAT/dev/stdin -- "$$(<${devfile})" 1>/dev/null
.  endif
.endfor

create-root: create-disks
.if ${USE_SOFTRAID:L} == "yes"
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DISKDEVFILES} | paste -sd, -- -)" \
	    -c1C -k"$$(<${KEYDEVFILE})"a -- softraid0 | \
		awk -- '{ print $$NF }' 1>${ROOTDEVFILE}
	${SUDO} ${FORMAT_DISK} -- "$$(<${ROOTDEVFILE})" 1>/dev/null
.else
	ln -s -- ${DISKDEVFILES} ${ROOTDEVFILE}
.endif
	${SUDO} disklabel -Aw -- "$$(<${ROOTDEVFILE})"
	${SUDO} newfs -q -- "$$(<${ROOTDEVFILE})"a 1>/dev/null
	mkdir -p -- ${MOUNTPOINT}
	${SUDO} mount -- /dev/"$$(<${ROOTDEVFILE})"a ${MOUNTPOINT}
	${SUDO} mkdir -- ${MOUNTPOINT}/usr
	${SUDO} cp -r -- ${STAGEDIR} ${MOUNTPOINT}/usr/


# must be first as some targets expect a working filesystem
REGRESS_TARGETS = prepare

prepare:
	${SUDO} ${REAL_RUN} -p -- "$$(<${ROOTDEVFILE})"

REGRESS_TARGETS +=	dry-prepare \
			dry-default \
			dry-root \
			root \
			root-stages

dry-prepare:
	${SUDO} ${DRY_RUN} -p -- "$$(<${ROOTDEVFILE})"
dry-default:
	${SUDO} ${DRY_RUN} -- "$$(<${ROOTDEVFILE})"
dry-root:
	${SUDO} ${DRY_RUN} -r/ -- "$$(<${ROOTDEVFILE})"

root:
	${SUDO} ${REAL_RUN} -r ${MOUNTPOINT} "$$(<${ROOTDEVFILE})"
root-stages:
	${SUDO} ${REAL_RUN} -- "$$(<${ROOTDEVFILE})" ${STAGEFILES}


REGRESS_EXPECTED_FAILURES =	dry-prepare-root \
				dry-prepare-stages \
				dry-nodisk-stages \
				dry-toofew \
				dry-toomany
REGRESS_TARGETS +=		${REGRESS_EXPECTED_FAILURES}

dry-prepare-root:
	${DRY_RUN} -p -r/ -- "$$(<${ROOTDEVFILE})" 2>/dev/null
dry-prepare-stages:
	${DRY_RUN} -p -- "$$(<${ROOTDEVFILE})" ${STAGEFILES} 2>/dev/null

dry-nodisk-stages:
	${SUDO} ${DRY_RUN} -- ${STAGEFILES} 2>/dev/null

dry-toofew:
	${DRY_RUN} -- 2>/dev/null
dry-toomany:
	${DRY_RUN} -- disk stage1 stage2 too many 2>/dev/null


CLEANFILES =		*.txt *.img
REGRESS_CLEANUP =	cleanup

# allow failure to always cleanup as much as possible
.IGNORE: cleanup
cleanup:
	${SUDO} umount -- ${MOUNTPOINT} 2>/dev/null
	rmdir -- ${MOUNTPOINT} 2>/dev/null
.if ${USE_SOFTRAID:L} == "yes"
	${SUDO} bioctl -d -- "$$(<${ROOTDEVFILE})" 2>/dev/null
.endif
.for _ devfile in ${FILETUPLES}
	${SUDO} vnconfig -u -- "$$(<${devfile})" 2>/dev/null
.endfor

.include <bsd.regress.mk>

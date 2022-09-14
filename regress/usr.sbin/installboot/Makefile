#	$OpenBSD: Makefile,v 1.29 2022/09/14 09:57:47 kn Exp $

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
FORMAT_DISK =		fdisk -iy -b '32768@32768:C'
STAGENAMES =		BOOTAA64.EFI
USE_SOFTRAID ?=		Yes
.elif ${MACHINE} == "armv7"
FORMAT_DISK =		fdisk -iy -b '32768@32768:C'
STAGENAMES =		BOOTARM.EFI
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "macppc"	# assume MBR
FORMAT_DISK =		fdisk -iy -b '2048@1:06'
STAGENAMES =		ofwboot
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "octeon"
FORMAT_DISK =		fdisk -iy -b '65536@64:C'
STAGENAMES =		boot
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "powerpc64"
FORMAT_DISK =		fdisk -iy -b '32768@32768:C'
STAGENAMES =		boot
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "riscv64"
FORMAT_DISK =		fdisk -iy -b '32768@32768:C'
STAGENAMES =		BOOTRISCV64.EFI
USE_SOFTRAID ?=		No
.elif ${MACHINE} == "sparc64"
FORMAT_DISK =		true	# NOOP
STAGENAMES =		bootblk ofwboot
USE_SOFTRAID ?=		Yes
.endif
# missing: alpha, hppa, landisk, loongson, luna88k

STAGEDIR =		/usr/mdec
STAGEFILES =		${STAGENAMES:=${STAGEDIR}/%}

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

REGRESS_SETUP_ONCE =	copy-bootstrap-to-root

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
.endfor

format-disks: create-disks
.for devfile in ${DISKDEVFILES}
	${SUDO} ${FORMAT_DISK} -- "$$(<${devfile})" 1>/dev/null
.  if ${USE_SOFTRAID:L} == "yes"
	echo 'RAID 1M-* 100%' | \
		${SUDO} disklabel -wAT/dev/stdin -- "$$(<${devfile})" 1>/dev/null
.  endif
.endfor
.if ${USE_SOFTRAID:L} == "yes"
	# XXX installboot(8) does not skip keydisks yet, so ensure the expected
	# MBR/GPT to avoid failing with
	# "installboot: invalid boot record signature (0x0000) @ sector 0"
	#
	# bioctl(8) and softraid(4) don't care about partition tables, they
	# just require a disklabel(5);  remove the next command or merge this
	# block into above DISKDEVFILES loop once installboot skips keydisks.
	${SUDO} ${FORMAT_DISK} -- "$$(<${KEYDEVFILE})" 1>/dev/null
	echo 'RAID 1M' | \
		${SUDO} disklabel -wAT/dev/stdin -- "$$(<${KEYDEVFILE})" 1>/dev/null
.endif

create-root: format-disks
.if ${USE_SOFTRAID:L} == "yes"
	${SUDO} bioctl -l"$$(sed -- s/$$/a/ ${DISKDEVFILES} | paste -sd, -- -)" \
	    -c1C -k"$$(<${KEYDEVFILE})"a -- softraid0 | \
		awk -- '{ print $$NF }' 1>${ROOTDEVFILE}
.else
	ln -s -- ${DISKDEVFILES} ${ROOTDEVFILE}
.endif

format-root: create-root
.if ${USE_SOFTRAID:L} == "yes"
	${SUDO} ${FORMAT_DISK} -- "$$(<${ROOTDEVFILE})" 1>/dev/null
.endif
	${SUDO} disklabel -Aw -- "$$(<${ROOTDEVFILE})"
	${SUDO} newfs -q -- "$$(<${ROOTDEVFILE})"a 1>/dev/null

mount-root: format-root
	mkdir -- ${MOUNTPOINT}
	${SUDO} mount -- /dev/"$$(<${ROOTDEVFILE})"a ${MOUNTPOINT}

copy-bootstrap-to-root: mount-root
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
	${SUDO} vnconfig -u -- "$$(<${KEYDEVFILE})" 2>/dev/null
.endif
.for devfile in ${DISKDEVFILES}
	${SUDO} vnconfig -u -- "$$(<${devfile})" 2>/dev/null
.endfor

.include <bsd.regress.mk>

#	$OpenBSD: Makefile,v 1.8 2018/10/23 14:25:43 bluhm Exp $

VMD ?= /usr/sbin/vmd

VMD_PASS=boot-keyword memory-round memory-just-enough cdrom-keyword \
	 format-keyword
VMD_FAIL=kernel-keyword too-few-ram vm-name-too-long too-many-ifs \
	 boot-name-too-long disk-path-too-long too-many-disks \
	 switch-no-interface switch-no-add cdrom-name-too-long \
	 bad-format missing-format

REGRESS_TARGETS=

.for n in ${VMD_PASS}
REGRESS_TARGETS +=	vmd-pass-${n}

vmd-pass-${n}:
	@echo '======== $@ ========'
	${VMD} -n -f ${.CURDIR}/vmd-pass-${n}.conf 2>&1 | \
	    diff -u ${.CURDIR}/vmd-pass-${n}.ok /dev/stdin
.endfor

.for n in ${VMD_FAIL}
REGRESS_TARGETS +=	vmd-fail-${n}

vmd-fail-${n}:
	@echo '======== $@ ========'
	${VMD} -n -f ${.CURDIR}/vmd-fail-${n}.conf 2>&1 | \
	    cut -d : -f 2,3,4 | \
	    diff -u ${.CURDIR}/vmd-fail-${n}.ok /dev/stdin
.endfor

PATH_LONG =	some/relativepath/somewhere/abcdefghijklmnopqrstuvwxyz0123456789/abcdefghijklmnopqrstuvwxyz0123456789/abcdefghijklmnopqrstuvwxyz0123456789
BOOT_PATH_LONG =	${PATH_LONG}/bsd.rd
DISK_PATH_LONG =	${PATH_LONG}/bsd.img

vmd-fail-boot-name-too-long: ${BOOT_PATH_LONG}
vmd-fail-disk-path-too-long: ${DISK_PATH_LONG}

${BOOT_PATH_LONG} ${DISK_PATH_LONG}:
	mkdir -p ${@:H}
	touch $@

clean:
	rm -rf some

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

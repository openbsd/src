#	$OpenBSD: Makefile,v 1.4 2018/01/07 22:59:57 ccardenas Exp $

VMD ?= /usr/sbin/vmd

VMD_PASS=boot-keyword memory-round memory-just-enough cdrom-keyword
VMD_FAIL=kernel-keyword too-few-ram vm-name-too-long too-many-ifs \
	 boot-name-too-long disk-path-too-long too-many-disks \
	 switch-no-interface switch-no-add cdrom-name-too-long

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

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

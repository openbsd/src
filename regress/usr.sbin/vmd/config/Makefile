#	$OpenBSD: Makefile,v 1.2 2017/10/30 03:49:30 mlarkin Exp $

VMD ?= /usr/sbin/vmd

VMD_PASS=boot-keyword memory-round memory-just-enough
VMD_FAIL=kernel-keyword too-few-ram vm-name-too-long too-many-ifs \
	 boot-name-too-long disk-path-too-long too-many-disks \
	 switch-no-interface

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

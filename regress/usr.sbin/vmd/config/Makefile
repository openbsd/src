#	$OpenBSD: Makefile,v 1.10 2020/12/16 22:59:55 bluhm Exp $

VMD ?= /usr/sbin/vmd

VMD_PASS=boot-keyword memory-round memory-just-enough cdrom-keyword \
	 format-keyword
VMD_FAIL=kernel-keyword too-few-ram vm-name-too-long too-many-ifs \
	 boot-name-too-long disk-path-too-long too-many-disks \
	 switch-no-interface switch-no-add cdrom-name-too-long \
	 bad-format missing-format

.for n in ${VMD_PASS}
REGRESS_TARGETS +=	run-pass-$n
run-pass-$n:
	${VMD} -n -f ${.CURDIR}/vmd-pass-$n.conf 2>&1 | \
	    diff -u ${.CURDIR}/vmd-pass-$n.ok /dev/stdin
.endfor

.for n in ${VMD_FAIL}
REGRESS_TARGETS +=	run-fail-$n
run-fail-$n:
	${VMD} -n -f ${.CURDIR}/vmd-fail-$n.conf 2>&1 | \
	    cut -d : -f 2,3,4 | \
	    diff -u ${.CURDIR}/vmd-fail-$n.ok /dev/stdin
.endfor

run-fail-too-many-disks: disk0.img disk1.img disk2.img disk3.img disk4.img
run-fail-bad-format run-fail-missing-format run-pass-format-keyword: foo.img

foo.img disk0.img disk1.img disk2.img disk3.img disk4.img:
	touch $@

PATH_LONG =	some/relativepath/somewhere/abcdefghijklmnopqrstuvwxyz0123456789/abcdefghijklmnopqrstuvwxyz0123456789/abcdefghijklmnopqrstuvwxyz0123456789
BOOT_PATH_LONG =	${PATH_LONG}/bsd.rd
DISK_PATH_LONG =	${PATH_LONG}/bsd.img

run-fail-boot-name-too-long: ${BOOT_PATH_LONG}
run-fail-disk-path-too-long: ${DISK_PATH_LONG}

${BOOT_PATH_LONG} ${DISK_PATH_LONG}:
	mkdir -p ${@:H}
	touch $@

clean:
	rm -f -- *.img
	rm -rf some

.include <bsd.regress.mk>

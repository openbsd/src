#	$OpenBSD: Makefile,v 1.4 2018/09/27 09:11:09 bluhm Exp $
#
# Copyright (c) 2010 Theo de Raadt <deraadt@openbsd.org>
# Copyright (c) 2018 Kenneth R Westerback <krw@openbsd.org>
# Copyright (c) 2018 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Regress tests for disklabel auto layout

DISK_SIZES =	  18000 \
		    256 \
		    400 \
		    800 \
		   1000 \
		   1024 \
		   2000 \
		   2200 \
		   2300 \
		   3000 \
		   3500 \
		   4000 \
		   4100 \
		   5000 \
		   6000 \
		   7000 \
		   8000 \
		   9000 \
		  90000 \
		  12000 \
		  20000 \
		  40000 \
		 300000 \
		 900000

DISKLABEL ?=	${.OBJDIR}/disklabel

CLEANFILES +=	*.out *.dl *.fstab diskimage disklabel.c manual.c

.PATH:		${.CURDIR}/../../../sbin/disklabel
PROG =		disklabel
SRCS =		disklabel.c dkcksum.c editor.c manual.c
CPPFLAGS =	-I ${.CURDIR}/../../../sbin/disklabel
LDADD =		-lutil

# The disk layout depends on physical ram.  Fake it to a fixed value.
# Recompile disklabel program.
disklabel.c: ../../../sbin/disklabel/disklabel.c Makefile
	sed '/^getphysmem/,/^}/s/^[	 ].*/	physmem = 16844521472ULL;/' \
	    ${.CURDIR}/../../../sbin/disklabel/disklabel.c >$@.tmp
	mv $@.tmp $@

manual.c:
	(echo 'const unsigned char manpage[] = {'; \
	echo 'no manual' | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c

.for s in ${DISK_SIZES}
REGRESS_TARGETS +=	run-disklabel-$s
run-disklabel-$s: ${DISKLABEL}
	@echo '\n======== $@ ========'
	${SUDO} vnconfig -u vnd0 || true
	rm -f diskimage
	echo "===== Start ${s}MB disk =====" >$s.out
	dd if=/dev/zero of=diskimage bs=1m count=1 seek=$s status=none
	${SUDO} vnconfig vnd0 diskimage
	${SUDO} fdisk -iy vnd0 >>$s.out
	echo >>$s.out
	${SUDO} ${DISKLABEL} -A -p g vnd0 >$s.dl
	${SUDO} ${DISKLABEL} -Aw -f $s.fstab vnd0 >>$s.dl
	${SUDO} ${DISKLABEL} vnd0 >>$s.dl
	grep '^  [a-p]: ' $s.dl | egrep -v "unused" | sort >>$s.out
	echo >>$s.out
	cat $s.fstab >>$s.out
	echo "===== End ${s}MB disk =====" >>$s.out
	diff -up ${.CURDIR}/$s.ok $s.out
.endfor

REGRESS_CLEANUP +=	cleanup
cleanup:
	@echo '\n======== $@ ========'
	-${SUDO} vnconfig -u vnd0
	rm -f diskimage

.include <bsd.regress.mk>

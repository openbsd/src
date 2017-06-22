# $OpenBSD: Makefile,v 1.2 2017/06/22 16:10:34 bluhm Exp $

# Copyright (c) 2008 Marco Pfatschbacher <mpf@openbsd.org>
# Copyright (c) 2017 Alexander Bluhm <bluhm@openbsd.org>
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

CLEANFILES=	*.gz rc rcmotd multi owner perm

# Test if gzip(1) detects truncated or corrupted files

REGRESS_TARGETS+=	run-regress-integrity
run-regress-integrity:
	@echo "\n==== $@ ===="
	# create gz
	gzip -v </etc/rc >rc.gz
	# check that everything is ok
	gzip -vt rc.gz

REGRESS_TARGETS+=	run-regress-truncate-2k
run-regress-truncate-2k:
	@echo "\n==== $@ ===="
	# truncate at 2k
	gzip </etc/rc >rc.gz
	dd if=rc.gz bs=1k count=2 of=2k.gz
	# check that truncation is detected
	! gzip -vt 2k.gz

REGRESS_TARGETS+=	run-regress-truncate-1k
run-regress-truncate-1k:
	@echo "\n==== $@ ===="
	# truncate at 1k
	gzip </etc/rc >rc.gz
	dd if=rc.gz bs=1k count=1 of=1k.gz
	# check that truncation is detected
	! gzip -vt 1k.gz

REGRESS_TARGETS+=	run-regress-skip
run-regress-skip:
	@echo "\n==== $@ ===="
	# skip some data in the middle
	gzip </etc/rc >rc.gz
	dd if=rc.gz bs=1k count=1 of=skip.gz
	dd if=rc.gz bs=1k seek=2 skip=2 conv=notrunc of=skip.gz
	# check that corruption is detected
	! gzip -vt skip.gz

JOT100!=	jot 100
REGRESS_TARGETS+=	run-regress-fuzz
run-regress-fuzz:
	@echo "\n==== $@ ===="
	# simple fuzzer that modifies one random byte at a random offset
	gzip </etc/rc >rc.gz
.for i in ${JOT100}
	dd if=rc.gz bs=1k of=fuzz.gz status=none
	RANDOM=$i;\
	    where=$$((RANDOM % 2048 + 256)); fuzz=$$((RANDOM % 256));\
	    orig=`dd if=rc.gz bs=1 skip=$$where count=1 status=none |\
	    hexdump -e '"%d"'`;\
	    echo "$i/100: fuzzing byte @$$where: $$orig -> $$fuzz";\
	    echo -n \\0`printf "%o" $$fuzz` |\
	    dd bs=1 seek=$$where conv=notrunc of=fuzz.gz status=none
	cmp -s rc.gz fuzz.gz || ! gzip -vt fuzz.gz
.endfor

# test basic gzip functionality

REGRESS_TARGETS+=	run-regress-gunzip
run-regress-gunzip:
	@echo "\n==== $@ ===="
	# gzip and gunzip
	gzip -v </etc/rc >rc.gz
	gunzip -f rc.gz
	# check that uncompressed file does match
	diff -up /etc/rc rc

REGRESS_TARGETS+=	run-regress-multi
run-regress-multi:
	@echo "\n==== $@ ===="
	# compress multiple files
	gzip -c /etc/rc /etc/motd >multi.gz
	# check multiple gzip file
	gzip -vt multi.gz
	gunzip -f multi.gz
	# check that gunzipped files do match
	cat /etc/rc /etc/motd >rcmotd
	diff -up rcmotd multi

# Test permissions

REGRESS_TARGETS+=	run-regress-perm-zip
run-regress-perm-zip:
	@echo "\n==== $@ ===="
	# compress file with special permissions
	cat /etc/rc >perm
	chmod 614 perm
	rm -f perm.gz
	gzip perm
	ls -l perm.gz | grep '^-rw---xr-- '

REGRESS_TARGETS+=	run-regress-perm-unzip
run-regress-perm-unzip:
	@echo "\n==== $@ ===="
	# uncompress file with special permissions
	gzip </etc/rc >perm.gz
	chmod 614 perm.gz
	rm -f perm
	gunzip perm.gz
	ls -l perm | grep '^-rw---xr-- '

REGRESS_TARGETS+=	run-regress-owner-zip
run-regress-owner-zip:
	@echo "\n==== $@ ===="
	# compress file as root with user and group nobody
	rm -f owner
	cat /etc/rc >owner
	${SUDO} chown nobody:nobody owner
	rm -f owner.gz
	${SUDO} gzip owner
	ls -l owner.gz | grep ' nobody  *nobody '

REGRESS_TARGETS+=	run-regress-owner-unzip
run-regress-owner-unzip:
	@echo "\n==== $@ ===="
	# uncompress file with special permissions
	rm -f owner.gz
	gzip </etc/rc >owner.gz
	${SUDO} chown nobody:nobody owner.gz
	rm -f owner
	${SUDO} gunzip owner.gz
	ls -l owner | grep ' nobody  *nobody '

.include <bsd.regress.mk>

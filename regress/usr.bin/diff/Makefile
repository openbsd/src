# $OpenBSD: Makefile,v 1.1 2003/07/17 21:04:03 otto Exp $

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10

DIFF=diff

# .1 and .2: 	input files

# t1: two empty files
# t2: basic editing
# t3: one empty, other contains text
# t4: one contains text, other empty
# t5: files are the same
# t6: one file with no newline at end, other does contain newline
# t7: one file with newline at end, the other not
# t8: revisison 1.1 and 1.54 of kern_malloc.c
# t9: revisison 1.1 and 1.104 of vfs_syscalls.c
# t10: two files with no end of line at the end, line in second file is longer

.SUFFIXES: .1 .2

all: clean ${REGRESS_TARGET}

.1:
	@cp ${.CURDIR}/${*}.1 ${*}.copy
	@cp ${.CURDIR}/${*}.1 ${*}.c.copy
	@cp ${.CURDIR}/${*}.1 ${*}.u.copy
	@${DIFF}  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.patch || true
	@${DIFF} -c  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.c.patch || true
	@${DIFF} -u  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.u.patch || true
	@patch -s ${*}.copy ${*}.patch || true
	@patch -s ${*}.c.copy ${*}.c.patch || true
	@patch -s ${*}.u.copy ${*}.u.patch || true
	@cmp -s ${.CURDIR}/${*}.2 ${*}.copy || (echo "XXX ${*} standard diff failed" && false)
	@cmp -s ${.CURDIR}/${*}.2 ${*}.c.copy || (echo "XXX ${*} context diff failed" && false)
	@cmp -s ${.CURDIR}/${*}.2 ${*}.u.copy || (echo "XXX ${*} unified diff failed" && false)
	
# Clean all files generated
clean:
	rm -f *.copy *.patch *.orig

.include <bsd.regress.mk>

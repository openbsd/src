#	$OpenBSD: Makefile,v 1.2 2018/09/25 12:15:49 bluhm Exp $
#
# Regress tests for disklabel

REGRESS_TARGETS =	run-regress-disklabel-A

CLEANFILES +=	disklabel-A-output disklabel-A-f image disklabel.c manual.c

.PATH:		${.CURDIR}/../../../sbin/disklabel
PROG =		disklabel
SRCS =		disklabel.c dkcksum.c editor.c manual.c
CPPFLAGS =	-I ${.CURDIR}/../../../sbin/disklabel
LDADD =		-lutil

# The disk layout depends on physical ram.  Fake it to a fixed value.
# Recompile disklabel program.
disklabel.c: ../../../sbin/disklabel/disklabel.c Makefile
	sed '/^getphysmem/,/^}/s/^[ 	].*/	physmem = 16844521472ULL;/' \
	    ${.CURDIR}/../../../sbin/disklabel/disklabel.c >$@.tmp
	mv $@.tmp $@

manual.c:
	(echo 'const unsigned char manpage[] = {'; \
	echo 'no manual' | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c

run-regress-disklabel-A: disklabel
	${SUDO} sh ${.CURDIR}/disklabel-A
	diff -up ${.CURDIR}/disklabel-A-expected disklabel-A-output

.include <bsd.regress.mk>

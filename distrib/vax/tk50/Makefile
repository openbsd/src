#	$OpenBSD: Makefile,v 1.2 2000/03/01 22:10:15 todd Exp $
#	$NetBSD: Makefile,v 1.1 1995/10/01 21:22:45 ragge Exp $
#
#	Create TK50 files and tapes.

TAPEDEV=mt8

.include "../Makefile.inc"

all:
	cp ../inst/bootroot-${REV}.fs tk50-file1-${REV}
	cp ../inst/miniroot-${REV}.fs tk50-file2-${REV}

tape:
.if !defined(DESTDIR)
	@echo 'DESTDIR' must be defined \(will make distribution\).
	@false

.else
	mt -f /dev/r${TAPEDEV} rewind
	dd if=tk50-file1-${REV} of=/dev/nr${TAPEDEV} bs=512
	dd if=tk50-file2-${REV} of=/dev/nr${TAPEDEV} bs=512
	dd if=${DESTDIR}/snapshot/bin.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/dev.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/etc.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/sbin.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.bin.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.games.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.include.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.lib.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.libexec.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.misc.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.sbin.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/usr.share.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	dd if=${DESTDIR}/snapshot/var.tar.gz of=/dev/nr${TAPEDEV} bs=64k
	mt -f /dev/r${TAPEDEV} rewind
	@echo Distribution tape is ready.
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

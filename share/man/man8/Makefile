#	$OpenBSD: Makefile,v 1.62 2007/08/01 21:23:27 miod Exp $
#	$NetBSD: Makefile,v 1.13 1996/03/28 21:36:40 mark Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

MAN=	afterboot.8 boot_config.8 compat_aout.8 compat_bsdos.8 \
	compat_freebsd.8 compat_hpux.8 compat_ibcs2.8 \
	compat_linux.8 compat_osf1.8 compat_sunos.8 \
	compat_svr4.8 compat_ultrix.8 crash.8 daily.8 dhcp.8 \
	diskless.8 genassym.sh.8 intro.8 netstart.8 rc.8 \
	rc.conf.8 rc.shutdown.8 release.8 security.8 ssl.8 \
	starttls.8 sticky.8 update.8 yp.8

MLINKS+=boot_config.8 UKC.8
MLINKS+=daily.8 weekly.8 daily.8 monthly.8
MLINKS+=rc.8 rc.local.8 rc.8 rc.securelevel.8
MLINKS+=rc.conf.8 rc.conf.local.8

SUBDIR=	man8.alpha man8.amd64 man8.armish man8.aviion man8.hp300 \
	man8.hppa man8.hppa64 man8.i386 man8.landisk man8.luna88k man8.mac68k \
	man8.macppc man8.mvme68k man8.mvme88k man8.sgi man8.sparc man8.sparc64 \
	man8.vax man8.zaurus

.include <bsd.prog.mk>

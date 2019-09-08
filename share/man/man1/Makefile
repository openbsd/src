#	$OpenBSD: Makefile,v 1.15 2019/09/08 17:13:44 daniel Exp $
#	$NetBSD: Makefile,v 1.4 1994/12/22 10:48:04 cgd Exp $

.include <bsd.own.mk>

MAN=	help.1 intro.1

.if ${BUILD_CLANG:L} == "yes"
MAN += clang-local.1
.endif
.if ${BUILD_GCC3:L} == "yes" || ${BUILD_GCC4:L} == "yes"
MAN += gcc-local.1
.endif

# ports tools doc
MAN+=	check-lib-depends.1 clean-old-distfiles.1 dpb-replay.1 dpb.1 \
	outdated-perl-ports.1 \
	pkg_check-problems.1 pkg_check-manpages.1 pkg_outdated.1 \
	pkg_subst.1 \
	port-getpkgpath-helper.1 port-resolve-lib-helper.1 \
	port-search-helper.1 \
	portbump.1 portcheck.1 portgen.1 \
	portimport.1 portlock.1 portslogger.1 proot.1 \
	register-plist.1 update-patches.1 update-plist.1

.include <bsd.prog.mk>

# $OpenBSD: Makefile,v 1.22 2005/03/01 21:47:19 espie Exp $

.include <bsd.own.mk>

MAN=pkg_add.1 pkg_info.1 pkg_create.1 pkg_delete.1 pkg.1 pkg_mklocatedb.1
POD2MAN=/usr/bin/pod2man

PACKAGES= \
    OpenBSD/Add.pm \
    OpenBSD/CollisionReport.pm \
    OpenBSD/Delete.pm \
    OpenBSD/Error.pm \
    OpenBSD/Getopt.pm \
    OpenBSD/IdCache.pm \
    OpenBSD/Mtree.pm \
    OpenBSD/PackageInfo.pm \
    OpenBSD/PackageLocator.pm \
    OpenBSD/PackageName.pm \
    OpenBSD/PackingElement.pm \
    OpenBSD/PackingList.pm \
    OpenBSD/PackingOld.pm \
    OpenBSD/PkgCfl.pm \
    OpenBSD/PkgSpec.pm \
    OpenBSD/ProgressMeter.pm \
    OpenBSD/RequiredBy.pm \
    OpenBSD/SharedItems.pm \
    OpenBSD/SharedLibs.pm \
    OpenBSD/Temp.pm \
    OpenBSD/Update.pm \
    OpenBSD/Ustar.pm \
    OpenBSD/Vstat.pm \
    OpenBSD/md5.pm \


SCRIPTS= \
    pkg_add \
    pkg_info \
    pkg_create \
    pkg_delete \
    pkg_mklocatedb \
    pkg

LIBBASE=/usr/libdata/perl5

PODS= \
    OpenBSD::IdCache \
    OpenBSD::Mtree \
    OpenBSD::PackingElement \
    OpenBSD::PackingList \
    OpenBSD::PkgCfl \
    OpenBSD::PkgSpec \
    OpenBSD::Ustar \
    OpenBSD::Vstat

_quick3p=

.for p in ${PODS}
.  ifndef NOMAN
MANALL+= $p.cat3p
MANSRCALL += $p.3p
MANSRC_COOKIE=stamp-mansrc.build
_quick3p+=${.CURDIR}/pod/$p.pod $p
.    ifdef MANPS
PSALL+= $p.ps3p
.    endif
.  else
MANSRC_COOKIE=
.  endif

$p.3p: pod/$p.pod
	${POD2MAN} --section=3p --official ${.ALLSRC} $@
.endfor
.ifndef NOMAN
MANALL+=${MAN:S/.1$/.cat1/}
.  ifdef MANPS
PSALL+= ${MAN:S/.1$/.ps1/}
.  endif
.endif

mansrc.build:
.if !empty(_quick3p)
	cd ${.OBJDIR} && ${POD2MAN} --section=3p --official ${_quick3p}
.endif

# Nothing to build
depend:
all: ${MANSRC_COOKIE}

stamp-mansrc.build:
	@cd ${.CURDIR} && exec ${MAKE} mansrc.build
	@touch $@

install: 
.for i in ${PACKAGES}
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/${i:H}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/$i ${DESTDIR}${LIBBASE}/$i
.endfor
.for i in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/$i ${DESTDIR}${BINDIR}/$i
.endfor

clean:

.if !defined(NOMAN)
install: maninstall
.include <bsd.man.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.PHONY: mansrc.build

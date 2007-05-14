# $OpenBSD: Makefile,v 1.43 2007/05/14 13:15:28 espie Exp $

.include <bsd.own.mk>

MAN1=pkg_add.1 pkg_info.1 pkg_create.1 pkg_delete.1 pkg_merge.1 \
	pkg.1 pkg_mklocatedb.1
MAN5=package.5
MAN=${MAN1} ${MAN5}

POD2MAN=/usr/bin/pod2man

PACKAGES= \
    OpenBSD/Add.pm \
    OpenBSD/ArcCheck.pm \
    OpenBSD/CollisionReport.pm \
    OpenBSD/Delete.pm \
    OpenBSD/Dependencies.pm \
    OpenBSD/Error.pm \
    OpenBSD/Getopt.pm \
    OpenBSD/IdCache.pm \
    OpenBSD/Interactive.pm \
    OpenBSD/Mtree.pm \
    OpenBSD/PackageInfo.pm \
    OpenBSD/PackageLocation.pm \
    OpenBSD/PackageLocator.pm \
    OpenBSD/PackageName.pm \
    OpenBSD/PackageRepository.pm \
    OpenBSD/PackageRepository/Installed.pm \
    OpenBSD/PackageRepository/SCP.pm \
    OpenBSD/PackageRepositoryList.pm \
    OpenBSD/PackingElement.pm \
    OpenBSD/PackingList.pm \
    OpenBSD/PkgCfl.pm \
    OpenBSD/PkgConfig.pm \
    OpenBSD/PkgSpec.pm \
    OpenBSD/ProgressMeter.pm \
    OpenBSD/Replace.pm \
    OpenBSD/RequiredBy.pm \
    OpenBSD/Search.pm \
    OpenBSD/SharedItems.pm \
    OpenBSD/SharedLibs.pm \
    OpenBSD/Temp.pm \
    OpenBSD/Update.pm \
    OpenBSD/Ustar.pm \
    OpenBSD/Vstat.pm \
    OpenBSD/md5.pm \

PACKAGEDIRS=OpenBSD  OpenBSD/PackageRepository

SCRIPTS= \
    pkg_add \
    pkg_info \
    pkg_create \
    pkg_delete \
    pkg_merge \
    pkg_mklocatedb \
    pkg

LIBBASE=/usr/libdata/perl5

PODS= \
    OpenBSD::Getopt \
    OpenBSD::IdCache \
    OpenBSD::Mtree \
    OpenBSD::PackageName \
    OpenBSD::PackingElement \
    OpenBSD::PackingList \
    OpenBSD::PkgCfl \
    OpenBSD::PkgSpec \
    OpenBSD::RequiredBy \
    OpenBSD::Search \
    OpenBSD::Ustar \
    OpenBSD::Vstat

_quick3p=

.for p in ${PODS}
.  ifndef NOMAN
MANALL+= $p.cat3p
MANSRCALL += $p.3p
MANSRC_COOKIE=stamp-mansrc.build
_quick3p+=$p.pod ${.OBJDIR}/$p.3p
.    ifdef MANPS
PSALL+= $p.ps3p
.    endif
.  else
MANSRC_COOKIE=
.  endif

$p.3p: pod/$p.pod
	${POD2MAN} --section=3p --official --name=$p ${.ALLSRC} $@
.endfor
.ifndef NOMAN
MANALL+=${MAN1:S/.1$/.cat1/}
MANALL+=${MAN5:S/.5$/.cat5/}
.  ifdef MANPS
PSALL+= ${MAN1:S/.1$/.ps1/}
PSALL+= ${MAN5:S/.5$/.ps5/}
.  endif
.endif

mansrc.build:
.if !empty(_quick3p)
	cd ${.CURDIR}/pod && ${POD2MAN} --section=3p --official ${_quick3p}
.endif

# Nothing to build
depend:
all: ${MANSRC_COOKIE}

stamp-mansrc.build:
	@cd ${.CURDIR} && exec ${MAKE} mansrc.build
	@touch $@

install: 
.for i in ${PACKAGEDIRS}
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBBASE}/$i
.endfor
.for i in ${PACKAGES}
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

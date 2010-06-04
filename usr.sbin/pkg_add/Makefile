# $OpenBSD: Makefile,v 1.62 2010/06/04 13:19:39 espie Exp $

.include <bsd.own.mk>

MAN1=pkg_add.1 pkg_info.1 pkg_create.1 pkg_delete.1 pkg_merge.1 \
	pkg.1 pkg_mklocatedb.1
MAN5=package.5
MAN=${MAN1} ${MAN5}
NOPROG=

POD2MAN=/usr/bin/pod2man

PACKAGES= \
    OpenBSD/Add.pm \
    OpenBSD/AddCreateDelete.pm \
    OpenBSD/AddDelete.pm \
    OpenBSD/ArcCheck.pm \
    OpenBSD/CollisionReport.pm \
    OpenBSD/Delete.pm \
    OpenBSD/Dependencies.pm \
    OpenBSD/Error.pm \
    OpenBSD/ForwardDependencies.pm \
    OpenBSD/Getopt.pm \
    OpenBSD/Handle.pm \
    OpenBSD/IdCache.pm \
    OpenBSD/Interactive.pm \
    OpenBSD/LibSpec.pm \
    OpenBSD/LibSpec/Build.pm \
    OpenBSD/Mtree.pm \
    OpenBSD/OldLibs.pm \
    OpenBSD/PackageInfo.pm \
    OpenBSD/PackageLocation.pm \
    OpenBSD/PackageLocator.pm \
    OpenBSD/PackageName.pm \
    OpenBSD/PackageRepository.pm \
    OpenBSD/PackageRepository/Installed.pm \
    OpenBSD/PackageRepository/SCP.pm \
    OpenBSD/PackageRepository/Source.pm \
    OpenBSD/PackageRepositoryList.pm \
    OpenBSD/PackingElement.pm \
    OpenBSD/PackingList.pm \
    OpenBSD/Paths.pm \
    OpenBSD/PkgAdd.pm \
    OpenBSD/PkgCfl.pm \
    OpenBSD/PkgConfig.pm \
    OpenBSD/PkgCreate.pm \
    OpenBSD/PkgDelete.pm \
    OpenBSD/PkgInfo.pm \
    OpenBSD/PkgSpec.pm \
    OpenBSD/ProgressMeter/Term.pm \
    OpenBSD/ProgressMeter.pm \
    OpenBSD/Replace.pm \
    OpenBSD/RequiredBy.pm \
    OpenBSD/Search.pm \
    OpenBSD/Signature.pm \
    OpenBSD/SharedItems.pm \
    OpenBSD/SharedLibs.pm \
    OpenBSD/Subst.pm \
    OpenBSD/Temp.pm \
    OpenBSD/Tracker.pm \
    OpenBSD/Update.pm \
    OpenBSD/UpdateSet.pm \
    OpenBSD/Ustar.pm \
    OpenBSD/Vstat.pm \
    OpenBSD/md5.pm \
    OpenBSD/x509.pm

PACKAGEDIRS=OpenBSD  OpenBSD/PackageRepository OpenBSD/ProgressMeter \
    OpenBSD/LibSpec

SCRIPTS= \
    pkg_merge \
    pkg_mklocatedb \
    pkg

SCRIPTS_LNK = \
    pkg pkg_add \
    pkg pkg_create \
    pkg pkg_delete \
    pkg pkg_info

LIBBASE=/usr/libdata/perl5

PODS= \
    OpenBSD::Getopt \
    OpenBSD::IdCache \
    OpenBSD::Intro \
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

all: ${MANSRC_COOKIE} .WAIT ${MANALL} ${PSALL}

stamp-mansrc.build:
	@cd ${.CURDIR} && exec ${MAKE} mansrc.build
	@touch $@

CLEANFILES= ${MANSRCALL} ${MANALL} stamp-mansrc.build

realinstall: 
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
.for i j in ${SCRIPTS_LNK}
	cd ${DESTDIR}${BINDIR} && ln -f $i $j
.endfor

.include <bsd.prog.mk>
.PHONY: mansrc.build

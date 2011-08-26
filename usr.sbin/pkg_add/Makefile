# $OpenBSD: Makefile,v 1.71 2011/08/26 09:02:30 espie Exp $

.include <bsd.own.mk>

MAN1=pkg_add.1 pkg_info.1 pkg_create.1 pkg_delete.1 \
	pkg.1 pkg_mklocatedb.1
MAN5=package.5 pkg.conf.5
MAN8=pkg_check.8
MAN=${MAN1} ${MAN5} ${MAN8}
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
    OpenBSD/Log.pm \
    OpenBSD/Mtree.pm \
    OpenBSD/OldLibs.pm \
    OpenBSD/PackageInfo.pm \
    OpenBSD/PackageLocation.pm \
    OpenBSD/PackageLocator.pm \
    OpenBSD/PackageName.pm \
    OpenBSD/PackageRepository.pm \
    OpenBSD/PackageRepository/HTTP.pm \
    OpenBSD/PackageRepository/Installed.pm \
    OpenBSD/PackageRepository/Persistant.pm \
    OpenBSD/PackageRepository/SCP.pm \
    OpenBSD/PackageRepository/Source.pm \
    OpenBSD/PackageRepositoryList.pm \
    OpenBSD/PackingElement.pm \
    OpenBSD/PackingList.pm \
    OpenBSD/Paths.pm \
    OpenBSD/PkgAdd.pm \
    OpenBSD/PkgCfl.pm \
    OpenBSD/PkgCheck.pm \
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
    OpenBSD/State.pm \
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
    pkg_mklocatedb \
    pkg

SCRIPTS_LNK = \
    pkg pkg_add \
    pkg pkg_check \
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
    OpenBSD::State \
    OpenBSD::Ustar \
    OpenBSD::Vstat

MAN3=
_quick3p=

.for p in ${PODS}
.  ifndef NOMAN
MAN3 += $p.3p
MANSRC_COOKIE=stamp-mansrc.build
_quick3p+=$p.pod ${.OBJDIR}/$p.3p
.  else
MANSRC_COOKIE=
.  endif

$p.3p: pod/$p.pod
	${POD2MAN} --section=3p --official --name=$p ${.ALLSRC} $@
.endfor

mansrc.build:
.if !empty(_quick3p)
	cd ${.CURDIR}/pod && ${POD2MAN} --section=3p --official ${_quick3p}
.endif

all: ${MANSRC_COOKIE}

stamp-mansrc.build:
	@cd ${.CURDIR} && exec ${MAKE} mansrc.build
	@touch $@

MAN += ${MAN3}

CLEANFILES= ${MAN3} stamp-mansrc.build

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

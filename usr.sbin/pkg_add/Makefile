# $OpenBSD: Makefile,v 1.18 2004/11/21 15:36:17 espie Exp $

MAN=pkg_add.1 pkg_info.1 pkg_create.1 pkg_delete.1 pkg.1

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
    pkg

LIBBASE=/usr/libdata/perl5

# Nothing to build
depend:
all:


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

.include <bsd.own.mk>

.if !defined(NOMAN)
install: maninstall
.include <bsd.man.mk>
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>

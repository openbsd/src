#	$OpenBSD: Makefile,v 1.2 2000/09/11 17:15:18 art Exp $

#
# This is hairy. libroken (originally libroken and libutil) has to be built
# before ydr. But libroken should be a part of libarla (because libarla uses
# libroken), but libarla needs ydr to be able to build. Yuck.
#
# To the future maintainer (if I get hit by a car or something): The structure
# is like this (everything in src, except the makefiles), because we want
# the upgrade process to be easy, without building and installing 5 different
# libs and without making a hairy Makefile.bsd-wrapper
#
# Anomalies:
#  - Man pages are not built from the main source beacuse of differing
#    path names.
#  - config.h and arla-version.h have to be pre-built and put in src/include.
#    config.h contains macros for endianness, but they are only used in one
#    place and will be replaced on OpenBSD. They have to be placed in
#    src/include because we want to reduce the memory footprint of mkdep
#    (yes, it's a real problem).
#

.if !make(install)
SUBDIR		+= libroken
SUBDIR		+= ydr
SUBDIR		+= libarla
.endif
SUBDIR		+= afsd fs vos pts

## To be able to make depend in libarla we have to have ydr (that needs
## libroken) and libroken used by gensysname in (libarla/Makefile.ko.inc)

beforedepend:
	(cd libroken && make depend && make)
	(cd ydr && make depend && make)

.include <bsd.dep.mk>
.include <bsd.subdir.mk>

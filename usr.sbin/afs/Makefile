#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/14 21:52:50 art Exp $

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

#! /bin/sh
#	$OpenBSD: mkskel.sh,v 1.2 1996/06/26 05:35:39 deraadt Exp $


cat <<!
/* File created from flex.skl via mkskel.sh */

#include "flexdef.h"

const char *skel[] = {
!

sed 's/\\/&&/g' $* | sed 's/"/\\"/g' | sed 's/.*/  "&",/'

cat <<!
  0
};
!

#! /bin/sh
#	$OpenBSD: mkskel.sh,v 1.2 2000/03/01 22:10:03 todd Exp $

# idea and sed lines taken straight from flex

cat <<!EOF
/* File created via mkskel.sh */

char *crunched_skel[] = {
!EOF

sed 's/\\/&&/g' $* | sed 's/"/\\"/g' | sed 's/.*/  "&",/'

cat <<!EOF
  0
};
!EOF

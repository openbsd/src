#!/bin/sh
# This script uses rman (Rosetta Man), which complements TkMan, to strip
# nroff headers from a manpage file, and format the result into a VMS help
# file.
for name in $*
do
	NAME=`echo $name |sed -e 's/\.man$/.1/'`
	(echo 1 `echo $NAME | sed -e 's/\..*$//' | tr '[a-z]' '[A-Z]'` ; \
	(echo '.hy 0'; cat $name) |\
	nroff -Tascii -man |\
	rman -n$NAME |\
	sed	-e 's/^[1-9].*$//' \
		-e 's/^\([A-Z]\)/2 \1/' |\
	uniq)
done

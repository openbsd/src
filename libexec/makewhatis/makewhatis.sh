#! /bin/sh
#
# Written by J.T. Conklin <jtc@netbsd.org>.
# Public domain.
#

TDIR=/tmp/whatis$$
FILE=$TDIR/whatis

um=`umask`
umask 022
if ! mkdir $TDIR ; then
	printf "tmp directory %s already exists, looks like:\n" $TDIR
	ls -alF $TDIR
	exit 1
fi
umask $um

trap "rm -rf $TDIR; exit 1" 1 2 15

MANDIR=${1-/usr/share/man}
if test ! -d "$MANDIR"; then 
	echo "makewhatis: $MANDIR: not a directory"
	exit 1
fi

find $MANDIR -type f -name '*.0' -print | while read file
do
	sed -n -f /usr/share/man/makewhatis.sed $file;
done > $FILE

find $MANDIR -type f -name '*.0.Z' -print | while read file
do
	zcat $file | sed -n -f /usr/share/man/makewhatis.sed;
done >> $FILE

find $MANDIR -type f -name '*.0.gz' -print | while read file
do
	gzip -dc $file | sed -n -f /usr/share/man/makewhatis.sed;
done >> $FILE

sort -u -o $FILE $FILE

install -o bin -g bin -m 444 $FILE "$MANDIR/whatis.db"
rm -rf $TDIR
exit 0

#! /bin/sh
#
# Written by J.T. Conklin <jtc@netbsd.org>.
# Public domain.
#

trap "rm -f /tmp/whatis$$; exit 1" 1 2 15

MANDIR=${1-/usr/share/man}
if test ! -d "$MANDIR"; then 
	echo "makewhatis: $MANDIR: not a directory"
	exit 1
fi

find $MANDIR -type f -name '*.0' -print | while read file
do
	sed -n -f /usr/share/man/makewhatis.sed $file;
done > /tmp/whatis$$

find $MANDIR -type f -name '*.0.Z' -print | while read file
do
	zcat $file | sed -n -f /usr/share/man/makewhatis.sed;
done >> /tmp/whatis$$

find $MANDIR -type f -name '*.0.gz' -print | while read file
do
	gzip -dc $file | sed -n -f /usr/share/man/makewhatis.sed;
done >> /tmp/whatis$$

sort -u -o /tmp/whatis$$ /tmp/whatis$$

install -o bin -g bin -m 444 /tmp/whatis$$ "$MANDIR/whatis.db"
exit 0

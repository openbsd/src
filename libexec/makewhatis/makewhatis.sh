#! /bin/sh
#
# written by matthew green <mrg@eterna.com.au>, based on the
# original by J.T. Conklin <jtc@netbsd.org> and Thorsten
# Frueauf <frueauf@ira.uka.de>.
#
# Public domain.
#
# $OpenBSD: makewhatis.sh,v 1.5 1997/11/13 04:40:15 millert Exp $
#

PATH=/usr/bin:/bin; export PATH

# Create temp files safely
LIST=`mktemp ${TMPDIR=/tmp}/makewhatislist.XXXXXXXXXX` || exit 1
WHATIS=`mktemp ${TMPDIR=/tmp}/whatis.XXXXXXXXXX` || {
	rm -f $LIST
	exit 1
}
trap "rm -f $LIST $WHATIS; exit 1" 1 2 15

MANDIR=${1-/usr/share/man}
if test ! -d "$MANDIR"; then 
	echo "makewhatis: $MANDIR: not a directory"
	rm -f $LIST $WHATIS
	exit 1
fi

find $MANDIR \( -type f -o -type l \) -name '*.[0-9]*' -ls | \
    sort -n | awk '{if (u[$1]) next; u[$1]++ ; print $11}' > $LIST
 
egrep '\.[1-9]$' $LIST | xargs /usr/libexec/getNAME | \
	sed -e 's/ [a-zA-Z0-9]* \\-/ -/' >> $WHATIS

egrep '\.0$' $LIST | while read file
do
	sed -n -f /usr/share/man/makewhatis.sed $file;
done > $WHATIS

egrep '\.[0].(gz|Z)$' $LIST | while read file
do
	gzip -fdc $file | sed -n -f /usr/share/man/makewhatis.sed;
done >> $WHATIS

sort -u -o $WHATIS $WHATIS

install -o root -g bin -m 444 $WHATIS "$MANDIR/whatis.db"
rm -f $LIST $WHATIS
exit 0

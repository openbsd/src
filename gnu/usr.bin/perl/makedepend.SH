case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting makedepend (with variable substitutions)"
rm -f makedepend
$spitshell >makedepend <<!GROK!THIS!
$startsh
# makedepend.SH
#
## To use an alternate make, set \$altmake in config.sh.
MAKE=${altmake-make}
!GROK!THIS!
$spitshell >>makedepend <<'!NO!SUBS!'

export PATH || (echo "OOPS, this isn't sh.  Desperation time.  I will feed myself to sh."; sh \$0; kill \$\$)

case $CONFIG in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac

# We need .. when we are in the x2p directory if we are using the
# cppstdin wrapper script.
# Put .. and . first so that we pick up the present cppstdin, not
# an older one lying about in /usr/local/bin.
PATH=".:..:$PATH"
export PATH

$cat /dev/null >.deptmp
$rm -f *.c.c c/*.c.c
if test -f Makefile; then
    cp Makefile $firstmakefile
fi
mf=$firstmakefile
if test -f $mf; then
    defrule=`<$mf sed -n		\
	-e '/^\.c\(\$(OBJ_EXT)\|\.o\):.*;/{'	\
	-e    's/\$\*\.c//'		\
	-e    's/^[^;]*;[	 ]*//p'	\
	-e    q				\
	-e '}'				\
	-e '/^\.c\(\$(OBJ_EXT)\|\.o\): *$/{'	\
	-e    N				\
	-e    's/\$\*\.c//'		\
	-e    's/^.*\n[	 ]*//p'		\
	-e    q				\
	-e '}'`
fi
case "$defrule" in
'') defrule='$(CC) -c $(CFLAGS)' ;;
esac

: Create files in UU directory to avoid problems with long filenames
: on systems with 14 character filename limits so file.c.c and file.c
: might be identical
$test -d UU || mkdir UU

$MAKE clist || ($echo "Searching for .c files..."; \
	$echo *.c | $tr ' ' '\012' | $egrep -v '\*' >.clist)
for file in `$cat .clist`; do
# for file in `cat /dev/null`; do
    case "$file" in
    *.c) filebase=`basename $file .c` ;;
    *.y) filebase=`basename $file .y` ;;
    esac
    case "$file" in
    */*) finc="-I`echo $file | sed 's#/[^/]*$##`" ;;
    *)   finc= ;;
    esac
    $echo "Finding dependencies for $filebase$obj_ext."
    ( $echo "#line 1 \"$file\""; \
      $sed -n <$file \
	-e "/^${filebase}_init(/q" \
	-e '/^#line/d' \
	-e '/^#/{' \
	-e 's|/\*.*$||' \
	-e 's|\\$||' \
	-e p \
	-e '}' ) >UU/$file.c
    $cppstdin $finc -I/usr/local/include -I. $cppflags $cppminus <UU/$file.c |
    $sed \
	-e '/^#.*<stdin>/d' \
	-e 's#\.[0-9][0-9]*\.c#'"$file.c#" \
	-e 's/^[	 ]*#[	 ]*line/#/' \
	-e '/^# *[0-9][0-9]* *[".\/]/!d' \
	-e 's/^.*"\(.*\)".*$/'$filebase'\$(OBJ_EXT): \1/' \
	-e 's/^# *[0-9][0-9]* \(.*\)$/'$filebase'\$(OBJ_EXT): \1/' \
	-e 's|: \./|: |' \
	-e 's|\.c\.c|.c|' | \
    $uniq | $sort | $uniq >> .deptmp
done

$sed <$mf >$mf.new -e '1,/^# AUTOMATICALLY/!d'

$MAKE shlist || ($echo "Searching for .SH files..."; \
	$echo *.SH | $tr ' ' '\012' | $egrep -v '\*' >.shlist)

# Now extract the dependency on makedepend.SH
# (it should reside in the main Makefile):
mv .shlist .shlist.old
$egrep -v '^makedepend\.SH' <.shlist.old >.shlist
rm .shlist.old

if $test -s .deptmp; then
    for file in `cat .shlist`; do
	$echo `$expr X$file : 'X\(.*\).SH'`: $file $TOP/config.sh \; \
	    /bin/sh $file >> .deptmp
    done
    $echo "Updating $mf..."
    $echo "# If this runs make out of memory, delete /usr/include lines." \
	>> $mf.new
    $sed 's|^\(.*\(\$(OBJ_EXT)\|\.o\):\) *\(.*/.*\.c\) *$|\1 \3; '"$defrule \2|" .deptmp \
       >>$mf.new
else
    $MAKE hlist || ($echo "Searching for .h files..."; \
	$echo *.h | $tr ' ' '\012' | $egrep -v '\*' >.hlist)
    $echo "You don't seem to have a proper C preprocessor.  Using grep instead."
    $egrep '^#include ' `cat .clist` `cat .hlist`  >.deptmp
    $echo "Updating $mf..."
    <.clist $sed -n							\
	-e '/\//{'							\
	-e   's|^\(.*\)/\(.*\)\.c|\2\$(OBJ_EXT): \1/\2.c; '"$defrule \1/\2.c|p"	\
	-e   d								\
	-e '}'								\
	-e 's|^\(.*\)\.c|\1\$(OBJ_EXT): \1.c|p' >> $mf.new
    <.hlist $sed -n 's|\(.*/\)\(.*\)|s= \2= \1\2=|p' >.hsed
    <.deptmp $sed -n 's|c:#include "\(.*\)".*$|o: \1|p' | \
       $sed 's|^[^;]*/||' | \
       $sed -f .hsed >> $mf.new
    <.deptmp $sed -n 's|h:#include "\(.*\)".*$|h: \1|p' | \
       $sed -f .hsed >> $mf.new
    for file in `$cat .shlist`; do
	$echo `$expr X$file : 'X\(.*\).SH'`: $file $TOP/config.sh \; \
	    /bin/sh $file >> $mf.new
    done
fi
$rm -f $mf.old
$cp $mf $mf.old
$cp $mf.new $mf
$rm $mf.new
$echo "# WARNING: Put nothing here or make depend will gobble it up!" >> $mf
$rm -rf .deptmp UU .shlist .clist .hlist .hsed

!NO!SUBS!
$eunicefix makedepend
chmod +x makedepend
case `pwd` in
*SH)
    $rm -f ../makedepend
    ln makedepend ../makedepend
    ;;
esac

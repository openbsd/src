tdirs=`xfind . -type d -name 'wince-*'`
test "$tdirs" = "" || rm -rf $dirs
rm -f *.res *.pdb *~


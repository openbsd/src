cd h:/src

files="perl.c perlio.c perl.h"

for f in $files; do
  diff -c vc/perl/$f wince/perl/$f
done


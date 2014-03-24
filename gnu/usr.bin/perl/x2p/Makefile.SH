#!/bin/sh

case $PERL_CONFIG_SH in
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
*/Makefile.SH) cd `expr X$0 : 'X\(.*\)/'` ;;
Makefile.SH) ;;
*) case `pwd` in
   */x2p) ;;
   *) if test -d x2p; then cd x2p
      else echo "Can't figure out where to write output."; exit 1
	  fi;;
   esac;;
esac

echo "Extracting x2p/Makefile (with variable substitutions)"
rm -f Makefile

# The .PL extractions use the Cwd extension.  For statically-built
# perls, we need perl, not just miniperl.
case "$usedl" in
    define) perl="../miniperl" ;;
    *)      perl="../perl" ;;
esac

cat >Makefile <<!GROK!THIS!
# $RCSfile: Makefile.SH,v $$Revision: 4.1 $$Date: 92/08/07 18:29:07 $
#
# $Log:	Makefile.SH,v $

CC = $cc
BYACC = $byacc
LDFLAGS = $ldflags
shellflags = $shellflags

libs = $perllibs

$make_set_make
# grrr
SHELL = $sh

# These variables may need to be manually set for non-Unix systems.
AR = $ar
EXE_EXT = $_exe
LIB_EXT = $_a
OBJ_EXT = $_o
PATH_SEP = $p_

FIRSTMAKEFILE = $firstmakefile

# how to tr(anslate) newlines

TRNL = '$trnl'

OPTIMIZE = $optimize

.SUFFIXES: .c \$(OBJ_EXT)

RUN = $run
PERL = $perl

!GROK!THIS!

cat >>Makefile <<'!NO!SUBS!'

REALPERL = ../perl
CCCMD = `sh $(shellflags) cflags "optimize='$(OPTIMIZE)'" $@`

public = a2p$(EXE_EXT) s2p find2perl

private = 

manpages = a2p.man s2p.man

util =

sh = Makefile.SH cflags.SH
shextract = Makefile cflags

pl = find2perl.PL s2p.PL
plextract = find2perl s2p
plexe = find2perl.exe s2p.exe
plc   = find2perl.c s2p.c
plm   = a2p.loadmap

addedbyconf = $(shextract) $(plextract)

h = EXTERN.h INTERN.h ../config.h ../handy.h hash.h a2p.h str.h util.h

c = hash.c str.c util.c walk.c

obj = hash$(OBJ_EXT) str$(OBJ_EXT) util$(OBJ_EXT) walk$(OBJ_EXT)

lintflags = -phbvxac


.c$(OBJ_EXT):
	$(CCCMD) -DPERL_FOR_X2P $*.c

all: $(public) $(private) $(util)
	@echo " "

a2p$(EXE_EXT): $(obj) a2p$(OBJ_EXT)
	$(CC) -o a2p $(LDFLAGS) $(obj) a2p$(OBJ_EXT) $(libs)

# I now supply a2p.c with the kits, so the following section is
# used only if you force byacc to run by saying
#    make run_byacc
# byacc 1.8.2 or 1.9 are recommended.

run_byacc:	FORCE
	@ echo Expect many shift/reduce and reduce/reduce conflicts
	$(BYACC) a2p.y
	rm -f a2p.c
	sed -e 's/(yyn = yydefred\[yystate\])/((yyn = yydefred[yystate]))/' \
	    -e 's/(yys = getenv("YYDEBUG"))/((yys = getenv("YYDEBUG")))/' \
	    -e 's/^yyerrlab://' \
	    -e 's/^    goto yyerrlab;//' \
	    -e 's/^yynewerror://' \
	    -e 's/^    goto yynewerror;//' \
	    -e 's|^static char yysccsid\(.*\)|/* static char yysccsid\1 */|' \
	    -e 's/^\(char \*yyname\[\]\)/const \1/' \
	    -e 's/^\(char \*yyrule\[\]\)/const \1/' \
	    -e 's/^\(    register\) \(char \*yys;\)/\1 const \2/' \
	    < y.tab.c > a2p.c

FORCE:

# We don't want to regenerate a2p.c, but it might appear out-of-date
# after a patch is applied or a new distribution is made.
a2p.c: a2p.y
	-@sh -c true

a2p$(OBJ_EXT): a2p.c a2py.c a2p.h EXTERN.h util.h INTERN.h \
		../handy.h ../config.h str.h hash.h
	$(CCCMD) a2p.c

clean:
	rm -f a2p$(EXE_EXT) psed *$(OBJ_EXT) $(plexe) $(plc) $(plm)

distclean: veryclean

realclean: clean
	-rmdir .depending
	rm -f core $(addedbyconf) all
	rm -f $(FIRSTMAKEFILE) $(FIRSTMAKEFILE).old makefile.old

veryclean: realclean
	rm -f *~ *.orig

# The following lint has practically everything turned on.  Unfortunately,
# you have to wade through a lot of mumbo jumbo that can't be suppressed.
# If the source file has a /*NOSTRICT*/ somewhere, ignore the lint message
# for that spot.

lint:
	lint $(lintflags) $(defs) $(c) > a2p.fuzz

depend: ../makedepend
	sh ../makedepend MAKE=$(MAKE)

clist:
	echo $(c) | tr ' ' $(TRNL) >.clist

hlist:
	echo $(h) | tr ' ' $(TRNL) >.hlist

shlist:
	echo $(sh) | tr ' ' $(TRNL) >.shlist

$(plextract):
	$(RUN) $(PERL) -I../lib $@.PL

find2perl: find2perl.PL ../config.sh

s2p: s2p.PL ../config.sh

# AUTOMATICALLY GENERATED MAKE DEPENDENCIES--PUT NOTHING BELOW THIS LINE
$(obj):
	@ echo "You haven't done a "'"make depend" yet!'; exit 1
makedepend: depend
!NO!SUBS!
$eunicefix Makefile
case `pwd` in
*SH)
    $rm -f ../Makefile
    $ln Makefile ../Makefile
    ;;
esac
rm -f $firstmakefile

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

case "$d_dosuid" in
*define*) suidperl='suidperl' ;;
*) suidperl='';;
esac

shrpenv=""
case "$d_shrplib" in
*define*)
    patchlevel=`egrep '^#define[ 	]+PATCHLEVEL' patchlevel.h \
	| awk '{print $3}'`
    case "$patchlevel" in
     *[0-9]) plibsuf=.$so.$patchlevel;;
     *)	    plibsuf=.$so;;
    esac
    if test "x$plibext" != "x" ; then  plibsuf=$plibext d_shrplib=custom ; fi
    case "$shrpdir" in
     /usr/lib)	;;
     "")	;;
     *)		shrpenv="env LD_RUN_PATH=$shrpdir";;
    esac
    pldlflags="$cccdlflags";;
*)  plibsuf=$lib_ext
    pldlflags="";;
esac

: Prepare dependency lists for Makefile.
dynamic_list=' '
for f in $dynamic_ext; do
    : the dependency named here will never exist
      base=`echo "$f" | sed 's/.*\///'`
    dynamic_list="$dynamic_list lib/auto/$f/$base.$dlext"
done

static_list=' '
static_ai_list=' '
for f in $static_ext; do
	base=`echo "$f" | sed 's/.*\///'`
	static_list="$static_list lib/auto/$f/$base\$(LIB_EXT)"
	if test -f ext/$f/AutoInit.c; then
	    static_ai_list="$static_ai_list ext/$f/AutoInit.c"
	fi
	if test -f ext/$f/AutoInit.pl; then
	    static_ai_list="$static_ai_list ext/$f/AutoInit.pl"
	fi
done

echo "Extracting Makefile (with variable substitutions)"
$spitshell >Makefile <<'!NO!SUBS!'
# Makefile.SH
# This file is derived from Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.
#  Makefile is used to generate makefile.  The only difference
#  is that makefile has the dependencies filled in at the end.
#
#
!NO!SUBS!

$spitshell >>Makefile <<!GROK!THIS!
# I now supply perly.c with the kits, so don't remake perly.c without byacc
BYACC = $byacc
CC = $cc
LD = $ld

LDFLAGS = $ldflags
CLDFLAGS = $ldflags

SMALL = $small
LARGE = $large $split
mallocsrc = $mallocsrc
mallocobj = $mallocobj
LNS = $lns
RMS = rm -f
ranlib = $ranlib

# The following are mentioned only to make metaconfig include the
# appropriate questions in Configure.  If you want to change these,
# edit config.sh instead, or specify --man1dir=/wherever on
# installman commandline.
bin = $installbin
scriptdir = $scriptdir
privlib = $installprivlib
man1dir = $man1dir
man1ext = $man1ext
man3dir = $man3dir
man3ext = $man3ext

# The following are used to build and install shared libraries for
# dynamic loading.
LDDLFLAGS = $lddlflags
CCDLFLAGS = $ccdlflags
DLSUFFIX = .$dlext
PLDLFLAGS = $pldlflags
PLIBSUF = $plibsuf
SHRPENV = $shrpenv

dynamic_ext = $dynamic_list
static_ext = $static_list
ext = \$(dynamic_ext) \$(static_ext)
static_ext_autoinit = $static_ai_list
DYNALOADER = lib/auto/DynaLoader/DynaLoader\$(LIB_EXT)

libs = $libs $cryptlib

public = perl $suidperl utilities translators

shellflags = $shellflags

## To use an alternate make, set \$altmake in config.sh.
MAKE = ${altmake-make}

# These variables will be used in a future version to make
# the make file more portable to non-unix systems.
AR = $ar
EXE_EXT = $exe_ext
LIB_EXT = $lib_ext
OBJ_EXT = $obj_ext
PATH_SEP = $path_sep

FIRSTMAKEFILE = $firstmakefile

# Any special object files needed by this architecture, e.g. os2/os2.obj
ARCHOBJS = $archobjs

.SUFFIXES: .c \$(OBJ_EXT)

!GROK!THIS!

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

CCCMD = `sh $(shellflags) cflags $(perllib) $@`

private = preplibrary lib/ExtUtils/Miniperl.pm lib/Config.pm

# Files to be built with variable substitution before miniperl
# is available.
sh = Makefile.SH cflags.SH config_h.SH makeaperl.SH makedepend.SH \
	makedir.SH perl_exp.SH writemain.SH

shextract = Makefile cflags config.h makeaperl makedepend \
	makedir perl_exp writemain

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = pod/pod2html.PL pod/pod2latex.PL pod/pod2man.PL pod/pod2text.PL

plextract = pod/pod2html pod/pod2latex pod/pod2man pod/pod2text

addedbyconf = UU $(shextract) $(plextract) pstruct

h1 = EXTERN.h INTERN.h XSUB.h av.h config.h cop.h cv.h dosish.h
h2 = embed.h form.h gv.h handy.h hv.h keywords.h mg.h op.h
h3 = opcode.h patchlevel.h perl.h perly.h pp.h proto.h regcomp.h
h4 = regexp.h scope.h sv.h unixish.h util.h
h = $(h1) $(h2) $(h3) $(h4)

c1 = $(mallocsrc) av.c scope.c op.c doop.c doio.c dump.c hv.c mg.c
c2 = perl.c perly.c pp.c pp_hot.c pp_ctl.c pp_sys.c regcomp.c regexec.c
c3 = gv.c sv.c taint.c toke.c util.c deb.c run.c globals.c

c = $(c1) $(c2) $(c3) miniperlmain.c perlmain.c

obj1 = $(mallocobj) gv$(OBJ_EXT) toke$(OBJ_EXT) perly$(OBJ_EXT) op$(OBJ_EXT) regcomp$(OBJ_EXT) dump$(OBJ_EXT) util$(OBJ_EXT) mg$(OBJ_EXT)
obj2 = hv$(OBJ_EXT) av$(OBJ_EXT) run$(OBJ_EXT) pp_hot$(OBJ_EXT) sv$(OBJ_EXT) pp$(OBJ_EXT) scope$(OBJ_EXT) pp_ctl$(OBJ_EXT) pp_sys$(OBJ_EXT)
obj3 = doop$(OBJ_EXT) doio$(OBJ_EXT) regexec$(OBJ_EXT) taint$(OBJ_EXT) deb$(OBJ_EXT) globals$(OBJ_EXT)
  
obj = $(obj1) $(obj2) $(obj3) $(ARCHOBJS)

# Once perl has been Configure'd and built ok you build different
# perl variants (Debugging, Embedded, Multiplicity etc) by saying:
#	make clean; make perllib=libperl<type>.a
# where <type> is some combination of 'd' and(or) 'e' or 'm'.
# See cflags to understand how this works.
#
# Eventually some form of 'make-a-perl' script will automate this
# together with linking a perl executable with any desired
# static modules.
perllib = libperl$(PLIBSUF)

lintflags = -hbvxac

# grrr
SHELL = /bin/sh

.c$(OBJ_EXT):
	$(CCCMD) $(PLDLFLAGS) $*.c

all: makefile miniperl $(private) $(plextract) $(public) $(dynamic_ext)
	@echo " "; echo "	Everything is up to date."

translators:	miniperl lib/Config.pm FORCE
	@echo " "; echo "	Making x2p stuff"; cd x2p; $(MAKE) all

utilities:	miniperl lib/Config.pm FORCE
	@echo " "; echo "	Making utilities"; cd utils; $(MAKE) all


# This is now done by installman only if you actually want the man pages.
#	@echo " "; echo "	Making docs"; cd pod; $(MAKE) all;

# Phony target to force checking subdirectories.
# Apparently some makes require an action for the FORCE target.
FORCE:
	@sh -c true

# The $& notation tells Sequent machines that it can do a parallel make,
# and is harmless otherwise.
# The miniperl -w -MExporter line is a basic cheap test to catch errors
# before make goes on to run preplibrary and then MakeMaker on extensions.
# This is very handy because later errors are often caused by miniperl
# build problems but that's not obvious to the novice.
# The Module used here must not depend on Config or any extensions.

miniperl: $& miniperlmain$(OBJ_EXT) $(perllib)
	$(CC) $(LARGE) $(CLDFLAGS) -o miniperl miniperlmain$(OBJ_EXT) $(perllib) $(libs)
	@./miniperl -w -Ilib -MExporter -e 0 || $(MAKE) minitest

miniperlmain$(OBJ_EXT): miniperlmain.c
	$(CCCMD) $(PLDLFLAGS) $*.c

perlmain.c: miniperlmain.c config.sh makefile $(static_ext_autoinit)
	sh writemain $(DYNALOADER) $(static_ext) > tmp
	sh mv-if-diff tmp perlmain.c

perlmain$(OBJ_EXT): perlmain.c
	$(CCCMD) $(PLDLFLAGS) $*.c

# The file ext.libs is a list of libraries that must be linked in
# for static extensions, e.g. -lm -lgdbm, etc.  The individual
# static extension Makefile's add to it.
ext.libs: $(static_ext)
	-@test -f ext.libs || touch ext.libs

perl: $& perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) ext.libs
	$(SHRPENV) $(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o perl perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) `cat ext.libs` $(libs)

pureperl: $& perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) ext.libs
	purify $(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o pureperl perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) `cat ext.libs` $(libs)

quantperl: $& perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) ext.libs
	quantify $(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o quantperl perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) `cat ext.libs` $(libs)

$(perllib): $& perl$(OBJ_EXT) $(obj)
!NO!SUBS!

case "$d_shrplib" in
*define*)
$spitshell >>Makefile <<'!NO!SUBS!'
	$(LD) $(LDDLFLAGS) -o $@ perl$(OBJ_EXT) $(obj)
!NO!SUBS!
;;
custom)
if test -r $osname/Makefile.SHs ; then 
  . $osname/Makefile.SHs
  $spitshell >>Makefile <<!GROK!THIS!

Makefile: $osname/Makefile.SHs

!GROK!THIS!
else
  echo "Could not find $osname/Makefile.SH! Skipping target \$(perllib) in Makefile!"
fi
;;
*)
$spitshell >>Makefile <<'!NO!SUBS!'
	rm -f $(perllib)
	$(AR) rcu $(perllib) perl$(OBJ_EXT) $(obj)
	@$(ranlib) $(perllib)
!NO!SUBS!
;;
esac

$spitshell >>Makefile <<'!NO!SUBS!'

# This version, if specified in Configure, does ONLY those scripts which need
# set-id emulation.  Suidperl must be setuid root.  It contains the "taint"
# checks as well as the special code to validate that the script in question
# has been invoked correctly.

suidperl: $& sperl$(OBJ_EXT) perlmain$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) ext.libs
	$(CC) $(LARGE) $(CLDFLAGS) $(CCDLFLAGS) -o suidperl perlmain$(OBJ_EXT) sperl$(OBJ_EXT) $(perllib) $(DYNALOADER) $(static_ext) `cat ext.libs` $(libs)

sperl$(OBJ_EXT): perl.c perly.h patchlevel.h $(h)
	$(RMS) sperl.c
	$(LNS) perl.c sperl.c
	$(CCCMD) -DIAMSUID sperl.c
	$(RMS) sperl.c

# We have to call our ./makedir because Ultrix 4.3 make can't handle the line
#	test -d lib/auto || mkdir lib/auto
#
preplibrary: miniperl lib/Config.pm $(plextract)
	@sh ./makedir lib/auto
	@echo "	AutoSplitting perl library"
	@./miniperl -Ilib -e 'use AutoSplit; \
		autosplit_lib_modules(@ARGV)' lib/*.pm lib/*/*.pm

# Take care to avoid modifying lib/Config.pm without reason
lib/Config.pm: config.sh miniperl
	./miniperl configpm tmp
	sh mv-if-diff tmp lib/Config.pm

lib/ExtUtils/Miniperl.pm: miniperlmain.c miniperl minimod.pl lib/Config.pm
	./miniperl minimod.pl > tmp && mv tmp $@

$(plextract):	miniperl lib/Config.pm
	./miniperl -Ilib $@.PL

install: all install.perl install.man

install.perl:	all installperl
	./perl installperl

install.man:	all installman
	./perl installman

# Not implemented yet.
#install.html:	all installhtml
#	./perl installhtml

# I now supply perly.c with the kits, so the following section is
# used only if you force byacc to run by saying
# 	make run_byacc
# Since we patch up the byacc output, the perly.fixer script needs
# to run with precisely the same version of byacc as I use.  You
# normally shouldn't remake perly.[ch].

run_byacc:	FORCE
	@ echo 'Expect' 130 shift/reduce and 1 reduce/reduce conflict
	$(BYACC) -d perly.y
	sh $(shellflags) ./perly.fixer y.tab.c perly.c
	mv y.tab.h perly.h
	echo 'extern YYSTYPE yylval;' >>perly.h
	- perl vms/vms_yfix.pl perly.c perly.h vms/perly_c.vms vms/perly_h.vms

# We don't want to regenerate perly.c and perly.h, but they might
# appear out-of-date after a patch is applied or a new distribution is
# made.
perly.c: perly.y
	-@sh -c true

perly.h: perly.y
	-@sh -c true

# The following three header files are generated automatically
#	keywords.h:	keywords.pl
#	opcode.h:	opcode.pl
#	embed.h:  	embed.pl global.sym interp.sym
# The correct versions should be already supplied with the perl kit,
# in case you don't have perl available.
# To force them to run, type
#	make regen_headers
regen_headers:	FORCE
	perl keywords.pl
	perl opcode.pl
	perl embed.pl

# Extensions:
# Names added to $(dynamic_ext) or $(static_ext) will automatically
# get built.  There should ordinarily be no need to change any of
# this part of makefile.
#
# The dummy dependency is a place holder in case $(dynamic_ext) or
# $(static_ext) is empty.
#
# DynaLoader may be needed for extensions that use Makefile.PL.

$(DYNALOADER):	miniperl preplibrary FORCE
	@sh ext/util/make_ext static $@ LIBPERL_A=$(perllib)

d_dummy $(dynamic_ext):	miniperl preplibrary $(DYNALOADER) FORCE
	@sh ext/util/make_ext dynamic $@ LIBPERL_A=$(perllib)

s_dummy $(static_ext):	miniperl preplibrary $(DYNALOADER) FORCE
	@sh ext/util/make_ext static $@ LIBPERL_A=$(perllib)

clean:
	rm -f *$(OBJ_EXT) *$(LIB_EXT) all perlmain.c
	rm -f perl.exp ext.libs
	-rm perl.export perl.dll perl.libexp perl.map perl.def
	-cd pod; $(MAKE) clean
	-cd utils; $(MAKE) clean
	-cd x2p; $(MAKE) clean
	-@for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) ; do \
	sh ext/util/make_ext clean $$x ; \
	done
	rm -f perl suidperl miniperl $(perllib)

realclean: clean
	-cd os2; rm -f Makefile
	-cd pod; $(MAKE) realclean
	-cd utils; $(MAKE) realclean
	-cd x2p; $(MAKE) realclean
	-@for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) ; do \
	sh ext/util/make_ext realclean $$x ; \
	done
	rm -f *.orig */*.orig *~ */*~ core t/core t/c t/perl
	rm -rf $(addedbyconf)
	rm -f $(FIRSTMAKEFILE) $(FIRSTMAKEFILE).old
	rm -f $(private)
	rm -rf lib/auto
	rm -f lib/.exists
	rm -f h2ph.man pstruct
	rm -rf .config
	@echo "Note that make realclean does not delete config.sh"

clobber:	realclean
	rm -f config.sh cppstdin

distclean:	clobber

# The following lint has practically everything turned on.  Unfortunately,
# you have to wade through a lot of mumbo jumbo that can't be suppressed.
# If the source file has a /*NOSTRICT*/ somewhere, ignore the lint message
# for that spot.

lint: perly.c $(c)
	lint $(lintflags) $(defs) perly.c $(c) > perl.fuzz

# Need to unset during recursion to go out of loop

MAKEDEPEND = makedepend

$(FIRSTMAKEFILE):	Makefile $(MAKEDEPEND)
	$(MAKE) depend MAKEDEPEND=

config.h: config.sh
	/bin/sh config_h.SH

# When done, touch perlmain.c so that it doesn't get remade each time.
depend: makedepend
	sh ./makedepend
	- test -s perlmain.c && touch perlmain.c
	cd x2p; $(MAKE) depend

# Cannot postpone this until $firstmakefile is ready ;-)
makedepend: makedepend.SH config.sh
	sh ./makedepend.SH

test: miniperl perl preplibrary $(dynamic_ext)
	- cd t && chmod +x TEST */*.t
	- cd t && (rm -f perl$(EXE_EXT); $(LNS) ../perl$(EXE_EXT) perl$(EXE_EXT)) && ./perl TEST </dev/tty

minitest: miniperl
	- cd t && chmod +x TEST */*.t
	- cd t && (rm -f perl$(EXE_EXT); $(LNS) ../miniperl$(EXE_EXT) perl$(EXE_EXT)) \
		&& ./perl TEST base/*.t comp/*.t cmd/*.t io/*.t op/*.t </dev/tty

clist:	$(c)
	echo $(c) | tr ' ' '\012' >.clist

hlist:  $(h)
	echo $(h) | tr ' ' '\012' >.hlist

shlist: $(sh)
	echo $(sh) | tr ' ' '\012' >.shlist

pllist: $(pl)
	echo $(pl) | tr ' ' '\012' >.pllist

# AUTOMATICALLY GENERATED MAKE DEPENDENCIES--PUT NOTHING BELOW THIS LINE
# If this runs make out of memory, delete /usr/include lines.
!NO!SUBS!

$eunicefix Makefile
case `pwd` in
*SH)
    $rm -f ../Makefile
    $ln Makefile ../Makefile
    ;;
esac
$rm -f $firstmakefile

#! /bin/sh
case $PERL_CONFIG_SH in
'')
	if test -f config.sh
		then TOP=.
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

linklibperl='$(LIBPERL)'
shrpldflags='$(LDDLFLAGS)'
ldlibpth=''
DPERL_EXTERNAL_GLOB='-DPERL_EXTERNAL_GLOB'
case "$useshrplib" in
true)
	# Prefix all runs of 'miniperl' and 'perl' with
	# $ldlibpth so that ./perl finds *this* shared libperl.
	case "$LD_LIBRARY_PATH" in
	'')
		ldlibpth="LD_LIBRARY_PATH=`pwd`";;
	*)
		ldlibpth="LD_LIBRARY_PATH=`pwd`:${LD_LIBRARY_PATH}";;
	esac

	pldlflags="$cccdlflags"
	static_target='static_pic'
	case "${osname}${osvers}" in
	next4*)
		ld=libtool
		lddlflags="-dynamic -undefined warning -framework System \
		-compatibility_version 1 -current_version $patchlevel \
		-prebind -seg1addr 0x27000000 -install_name \$(shrpdir)/\$@"
		;;
	rhapsody*|darwin*)
		shrpldflags="${ldflags} -dynamiclib \
                            -compatibility_version \
				${api_revision}.${api_version}.${api_subversion} \
			     -current_version \
				${revision}.${patchlevel}.${subversion} \
			     -install_name \$(shrpdir)/\$@"
		;;
	cygwin*)
		linklibperl="-lperl"
		;;
	sunos*)
		linklibperl="-lperl"
		;;
	netbsd*|freebsd[234]*|openbsd*)
		linklibperl="-L. -lperl"
		;;
	aix*)
		shrpldflags="-H512 -T512 -bhalt:4 -bM:SRE -bE:perl.exp"
		case "$osvers" in
		3*)	shrpldflags="$shrpldflags -e _nostart"
			;;
		*)	shrpldflags="$shrpldflags -b noentry"
			;;
		esac
	        shrpldflags="$shrpldflags $ldflags $perllibs $cryptlib"
		linklibperl="-L $archlibexp/CORE -L `pwd | sed 's/\/UU$//'` -lperl"
		;;
	hpux*)
		linklibperl="-L `pwd | sed 's/\/UU$//'` -Wl,+s -Wl,+b$archlibexp/CORE -lperl"
		;;
	os390*)
            shrpldflags='-W l,dll'
	    linklibperl='libperl.x'
	    DPERL_EXTERNAL_GLOB=''
	    ;;
	esac
	case "$ldlibpthname" in
	'') ;;
	*)
	    case "$osname" in
	    os2)
	        ldlibpth=''
	        ;;
	    *)
		eval "ldlibpth=\"$ldlibpthname=`pwd`:\$$ldlibpthname\""
		;;
	    esac
	    # Strip off any trailing :'s
	    ldlibpth=`echo $ldlibpth | sed 's/:*$//'`
	    ;;
        esac

	case "$ldlibpth" in
	# Protect any spaces
	*" "*) ldlibpth=`echo $ldlibpth|sed 's/ /\\\\ /g'` ;;
	esac

	case "$osname" in
	linux)
	    # If there is a pre-existing $libperl from a previous
	    # installation, Linux needs to use LD_PRELOAD to
	    # override the LD_LIBRARY_PATH setting.  See the
	    # INSTALL file, under "Building a shared perl library".
	    # If there is no pre-existing $libperl, we don't need
	    # to do anything further.
	    if test -f $archlib/CORE/$libperl; then
		rm -f preload
		cat <<'EOT' > preload
#! /bin/sh
lib=$1
shift
test -r $lib && export LD_PRELOAD="$lib $LD_PRELOAD"
exec "$@"
EOT
		chmod 755 preload
		ldlibpth="$ldlibpth `pwd`/preload `pwd`/$libperl"
	    fi
	    ;;
	os390)	test -f /bin/env && ldlibpth="/bin/env $ldlibpth"
		;;
	esac

	;;

*)	pldlflags=''
	static_target='static'
	;;
esac

: Prepare dependency lists for Makefile.
dynamic_list=' '
for f in $dynamic_ext; do
    : the dependency named here will never exist
      base=`echo "$f" | sed 's/.*\///'`
    dynamic_list="$dynamic_list lib/auto/$f/$base.$dlext"
done

static_list=' '
for f in $static_ext; do
	base=`echo "$f" | sed 's/.*\///'`
	static_list="$static_list lib/auto/$f/$base\$(LIB_EXT)"
done

nonxs_list=' '
for f in $nonxs_ext; do
    base=`echo "$f" | sed 's/.*\///'`
    nonxs_list="$nonxs_list ext/$f/pm_to_blib"
done

# Handle the usage of different yaccs in posix-bc (During Configure we
# us yacc for perly.y and byacc for a2p.y.  The makefiles must use the
# same configuration for run_byacc!):
case "$osname" in
    posix-bc)
	byacc=$yacc
	;;
esac

echo "Extracting Makefile (with variable substitutions)"
$spitshell >Makefile <<!GROK!THIS!
# Makefile.SH
# This file is derived from Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.
#  Makefile is used to generate $firstmakefile.  The only difference
#  is that $firstmakefile has the dependencies filled in at the end.
#
#
# I now supply perly.c with the kits, so don't remake perly.c without byacc
BYACC = $byacc
CC = $cc
LD = $ld

LDFLAGS = $ldflags
CLDFLAGS = $ldflags

mallocsrc = $mallocsrc
mallocobj = $mallocobj
LNS = $lns
# NOTE: some systems don't grok "cp -f". XXX Configure test needed?
CPS = $cp
RMS = rm -f
ranlib = $ranlib

# The following are mentioned only to make metaconfig include the
# appropriate questions in Configure.  If you want to change these,
# edit config.sh instead, or specify --man1dir=/wherever on
# installman commandline.
bin = $installbin
scriptdir = $scriptdir
shrpdir = $archlibexp/CORE
privlib = $installprivlib
man1dir = $man1dir
man1ext = $man1ext
man3dir = $man3dir
man3ext = $man3ext

# The following are used to build and install shared libraries for
# dynamic loading.
LDDLFLAGS = $lddlflags
SHRPLDFLAGS = $shrpldflags
CCDLFLAGS = $ccdlflags
DLSUFFIX = .$dlext
PLDLFLAGS = $pldlflags
LIBPERL = $libperl
LLIBPERL= $linklibperl
SHRPENV = $shrpenv

# Static targets are ordinarily built without CCCDLFLAGS.  However,
# if building a shared libperl.so that might later be linked into
# another application, then it might be appropriate to also build static
# extensions (usually just DynaLoader) with relocatable code (e.g. -fPIC
# for GNU cc).  This is handled by ext/util/make_ext.
STATIC = $static_target

# The following is used to include the current directory in
# the dynamic loader path you are building a shared libperl.
LDLIBPTH = $ldlibpth

dynamic_ext = $dynamic_list
static_ext = $static_list
nonxs_ext = $nonxs_list
ext = \$(dynamic_ext) \$(static_ext) \$(nonxs_ext)
DYNALOADER = lib/auto/DynaLoader/DynaLoader\$(LIB_EXT)

libs = $perllibs $cryptlib

public = perl\$(EXE_EXT) $suidperl utilities translators

shellflags = $shellflags

# This is set to  MAKE=$make if your $make command doesn't
# do it for you.
$make_set_make

# Mention $gmake here so it gets probed for by Configure.

# These variables may need to be manually set for non-Unix systems.
AR = $full_ar
EXE_EXT = $_exe
LIB_EXT = $_a
OBJ_EXT = $_o
PATH_SEP = $p_

FIRSTMAKEFILE = $firstmakefile

# Any special object files needed by this architecture, e.g. os2/os2.obj
ARCHOBJS = $archobjs

.SUFFIXES: .c \$(OBJ_EXT) .i .s

# grrr
SHELL = $sh

# how to tr(anslate) newlines
TRNL = '$trnl'

OPTIMIZE = $optimize

EXTRAS = $extras

INSTALLPREFIXEXP = $prefix

!GROK!THIS!
# not used by Makefile but by installperl;
# mentioned here so that metaconfig picks these up
# $installusrbinperl
# $versiononly

case "${osname}:${osvers}" in
darwin:*)
$spitshell >>Makefile <<EOF

# Your locales are broken (osname $osname, osvers $osvers)
# and to avoid the numerous
# perl: warning: Setting locale failed.
# warnings during the build process we reset the locale variables.

LC_ALL=C
LANG=C
LANGUAGE=C
EOF
	;;
esac

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

CCCMD    = `sh $(shellflags) cflags "optimize='$(OPTIMIZE)'" $@`

CCCMDSRC = `sh $(shellflags) cflags "optimize='$(OPTIMIZE)'" $<`

private = preplibrary lib/Config.pm lib/ExtUtils/Miniperl.pm

# Files to be built with variable substitution before miniperl
# is available.
sh = Makefile.SH cflags.SH config_h.SH makeaperl.SH makedepend.SH \
	makedir.SH myconfig.SH writemain.SH pod/Makefile.SH

shextract = Makefile cflags config.h makeaperl makedepend \
	makedir myconfig writemain pod/Makefile

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = pod/pod2html.PL pod/pod2latex.PL pod/pod2man.PL pod/pod2text.PL \
	pod/pod2usage.PL pod/podchecker.PL pod/podselect.PL

# lib/lib.pm is not listed here because it has a rule of its own.
plextract = pod/pod2html pod/pod2latex pod/pod2man pod/pod2text \
	pod/pod2usage pod/podchecker pod/podselect

addedbyconf = UU $(shextract) $(plextract) lib/lib.pm pstruct

h1 = EXTERN.h INTERN.h XSUB.h av.h config.h cop.h cv.h dosish.h
h2 = embed.h form.h gv.h handy.h hv.h keywords.h mg.h op.h opcode.h
h3 = pad.h patchlevel.h perl.h perlapi.h perly.h pp.h proto.h regcomp.h
h4 = regexp.h scope.h sv.h unixish.h util.h iperlsys.h thread.h
h5 = utf8.h warnings.h
h = $(h1) $(h2) $(h3) $(h4) $(h5)

c1 = $(mallocsrc) av.c scope.c op.c doop.c doio.c dump.c hv.c mg.c reentr.c
c2 = perl.c perly.c pp.c pp_hot.c pp_ctl.c pp_sys.c regcomp.c regexec.c utf8.c
c3 = gv.c sv.c taint.c toke.c util.c deb.c run.c universal.c xsutils.c pad.c
c4 = globals.c perlio.c perlapi.c numeric.c locale.c pp_pack.c pp_sort.c

c = $(c1) $(c2) $(c3) $(c4) miniperlmain.c perlmain.c

obj1 = $(mallocobj) gv$(OBJ_EXT) toke$(OBJ_EXT) perly$(OBJ_EXT) op$(OBJ_EXT) pad$(OBJ_EXT) regcomp$(OBJ_EXT) dump$(OBJ_EXT) util$(OBJ_EXT) mg$(OBJ_EXT) reentr$(OBJ_EXT)
obj2 = hv$(OBJ_EXT) av$(OBJ_EXT) run$(OBJ_EXT) pp_hot$(OBJ_EXT) sv$(OBJ_EXT) pp$(OBJ_EXT) scope$(OBJ_EXT) pp_ctl$(OBJ_EXT) pp_sys$(OBJ_EXT)
obj3 = doop$(OBJ_EXT) doio$(OBJ_EXT) regexec$(OBJ_EXT) utf8$(OBJ_EXT) taint$(OBJ_EXT) deb$(OBJ_EXT) universal$(OBJ_EXT) xsutils$(OBJ_EXT) globals$(OBJ_EXT) perlio$(OBJ_EXT) perlapi$(OBJ_EXT) numeric$(OBJ_EXT) locale$(OBJ_EXT) pp_pack$(OBJ_EXT) pp_sort$(OBJ_EXT)

obj = $(obj1) $(obj2) $(obj3) $(ARCHOBJS)

lintflags = -hbvxac

.c$(OBJ_EXT):
	$(CCCMD) $(PLDLFLAGS) $*.c

.c.i:
	$(CCCMDSRC) -E $*.c > $*.i

.c.s:
	$(CCCMDSRC) -S $*.c

all: $(FIRSTMAKEFILE) miniperl$(EXE_EXT) extra.pods $(private) $(public) $(dynamic_ext) $(nonxs_ext) extras.make
	@echo " ";
	@echo "	Everything is up to date. Type '$(MAKE) test' to run test suite."

.PHONY: all compile translators utilities

compile: all
	echo "testing compilation" > testcompile;
	cd utils;  $(MAKE) compile;
	cd x2p; $(MAKE) compile;
	cd pod; $(MAKE) compile;

translators:	miniperl$(EXE_EXT) lib/Config.pm FORCE
	@echo " "; echo "	Making x2p stuff"; cd x2p; $(LDLIBPTH) $(MAKE) all

utilities:	miniperl$(EXE_EXT) lib/Config.pm $(plextract) lib/lib.pm FORCE
	@echo " "; echo "	Making utilities"; cd utils; $(LDLIBPTH) $(MAKE) all


# This is now done by installman only if you actually want the man pages.
#	@echo " "; echo "	Making docs"; cd pod; $(MAKE) all;

# Phony target to force checking subdirectories.
# Apparently some makes require an action for the FORCE target.
.PHONY: FORCE
FORCE:
	@sh -c true
!NO!SUBS!
$spitshell >>Makefile <<!GROK!THIS!

# We do a copy of the op.c instead of a symlink because gcc gets huffy
# if we have a symlink forest to another disk (it complains about too many
# levels of symbolic links, even if we have only two)

opmini\$(OBJ_EXT): op.c config.h
	\$(RMS) opmini.c
	\$(CPS) op.c opmini.c
	\$(CCCMD) \$(PLDLFLAGS) $DPERL_EXTERNAL_GLOB opmini.c
	\$(RMS) opmini.c

!GROK!THIS!
$spitshell >>Makefile <<'!NO!SUBS!'
miniperlmain$(OBJ_EXT): miniperlmain.c patchlevel.h
	$(CCCMD) $(PLDLFLAGS) $*.c

perlmain.c: miniperlmain.c config.sh $(FIRSTMAKEFILE)
	sh writemain $(DYNALOADER) $(static_ext) > writemain.tmp
	sh mv-if-diff writemain.tmp perlmain.c

!NO!SUBS!
case "$osname" in
cygwin)
	;; # Let cygwin/Makefile.SHs do its work.
*)
	$spitshell >>Makefile <<'!NO!SUBS!'
perlmain$(OBJ_EXT): perlmain.c
	$(CCCMD) $(PLDLFLAGS) $*.c

!NO!SUBS!
	;;
esac
$spitshell >>Makefile <<'!NO!SUBS!'
# The file ext.libs is a list of libraries that must be linked in
# for static extensions, e.g. -lm -lgdbm, etc.  The individual
# static extension Makefile's add to it.
ext.libs: $(static_ext)
	-@test -f ext.libs || touch ext.libs

!NO!SUBS!

# How to build libperl.  This is still rather convoluted.
# Load up custom Makefile.SH fragment for shared loading and executables:
case "$osname" in
*)
	Makefile_s="$osname/Makefile.SHs"
	;;
esac

case "$osname" in
aix)
	$spitshell >>Makefile <<!GROK!THIS!
LIBS			= $perllibs
# In AIX we need to change this for building Perl itself from
# its earlier definition (which is for building external
# extensions *after* Perl has been built and installed)
CCDLFLAGS		= `echo $ccdlflags|sed -e 's@-bE:.*/perl\.exp@-bE:perl.exp@'`

!GROK!THIS!
	case "$useshrplib" in
	define|true|[yY]*)
		$spitshell >>Makefile <<'!NO!SUBS!'

LIBPERL_NONSHR		= libperl_nonshr$(LIB_EXT)
MINIPERL_NONSHR		= miniperl_nonshr$(EXE_EXT)

$(LIBPERL_NONSHR): perl$(OBJ_EXT) $(obj)
	$(RMS) $(LIBPERL_NONSHR)
	$(AR) rcu $(LIBPERL_NONSHR) perl$(OBJ_EXT) $(obj)

$(MINIPERL_NONSHR): $(LIBPERL_NONSHR) miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT)
	$(CC) $(LDFLAGS) -o $(MINIPERL_NONSHR) miniperlmain$(OBJ_EXT) \
	    opmini$(OBJ_EXT) $(LIBPERL_NONSHR) $(LIBS)

MINIPERLEXP		= $(MINIPERL_NONSHR)

LIBPERLEXPORT		= perl.exp

!NO!SUBS!
		
		;;
	*)	
		$spitshell >>Makefile <<'!NO!SUBS!'
MINIPERLEXP		= miniperl$(EXE_EXT)

PERLEXPORT		= perl.exp

!NO!SUBS!
	;;
	esac
	$spitshell >>Makefile <<'!NO!SUBS!'
perl.exp: $(MINIPERLEXP) makedef.pl config.sh $(SYM) $(SYMH)
	./$(MINIPERLEXP) makedef.pl PLATFORM=aix CC_FLAGS="$(OPTIMIZE)" | sort -u | sort -f > perl.exp.tmp
	sh mv-if-diff perl.exp.tmp perl.exp

!NO!SUBS!
	;;
os2)
	$spitshell >>Makefile <<'!NO!SUBS!'
MINIPERLEXP		= miniperl

perl5.def: $(MINIPERLEXP) makedef.pl config.sh $(SYM) $(SYMH) miniperl.map
	./$(MINIPERLEXP) makedef.pl PLATFORM=os2 -DPERL_DLL=$(PERL_DLL) CC_FLAGS="$(OPTIMIZE)" > perl.exp.tmp
	sh mv-if-diff perl.exp.tmp perl5.def

!NO!SUBS!
	;;
esac

if test -r $Makefile_s ; then
	. $Makefile_s
	$spitshell >>Makefile <<!GROK!THIS!

Makefile: $Makefile_s
!GROK!THIS!
else
	$spitshell >>Makefile <<'!NO!SUBS!'
$(LIBPERL): $& perl$(OBJ_EXT) $(obj) $(LIBPERLEXPORT)
!NO!SUBS!
	case "$useshrplib" in
	true)
		$spitshell >>Makefile <<'!NO!SUBS!'
	$(LD) -o $@ $(SHRPLDFLAGS) perl$(OBJ_EXT) $(obj) $(libs)
!NO!SUBS!
		case "$osname" in
		aix)
			$spitshell >>Makefile <<'!NO!SUBS!'
	rm -f libperl$(OBJ_EXT)
	mv $@ libperl$(OBJ_EXT)
	$(AR) qv $(LIBPERL) libperl$(OBJ_EXT)
!NO!SUBS!
			;;
		esac
		;;
	*)
		$spitshell >>Makefile <<'!NO!SUBS!'
	rm -f $(LIBPERL)
	$(AR) rcu $(LIBPERL) perl$(OBJ_EXT) $(obj)
	@$(ranlib) $(LIBPERL)
!NO!SUBS!
		;;
	esac
	$spitshell >>Makefile <<'!NO!SUBS!'

# How to build executables.

# The $& notation tells Sequent machines that it can do a parallel make,
# and is harmless otherwise.
# The miniperl -w -MExporter line is a basic cheap test to catch errors
# before make goes on to run preplibrary and then MakeMaker on extensions.
# This is very handy because later errors are often caused by miniperl
# build problems but that's not obvious to the novice.
# The Module used here must not depend on Config or any extensions.

!NO!SUBS!

	case "${osname}${osvers}" in
	aix*)
		$spitshell >>Makefile <<'!NO!SUBS!'
miniperl: $& miniperlmain$(OBJ_EXT) $(LIBPERL) opmini$(OBJ_EXT)
	$(CC) -o miniperl $(CLDFLAGS) \
	    `echo $(obj) | sed 's/ op$(OBJ_EXT) / /'` \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perl$(OBJ_EXT) $(libs)
	$(LDLIBPTH) ./miniperl -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	beos*|next4*)
		$spitshell >>Makefile <<'!NO!SUBS!'
miniperl: $& miniperlmain$(OBJ_EXT) $(LIBPERL) opmini$(OBJ_EXT)
	$(CC) -o miniperl `echo $(obj) | sed 's/ op$(OBJ_EXT) / /'` \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perl$(OBJ_EXT) $(libs)
	$(LDLIBPTH) ./miniperl -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	darwin*)
		case "$osvers" in
		[1-6].*) ;;
		*) case "$ldflags" in
		    *"-flat_namespace"*) ;;
		    *) # to allow opmini.o to override stuff in libperl.dylib
		$spitshell >>Makefile <<!NO!SUBS!
NAMESPACEFLAGS = -force_flat_namespace
!NO!SUBS!
		       ;;
		    esac
		    ;;
		esac
		$spitshell >>Makefile <<'!NO!SUBS!'
miniperl$(EXE_EXT): $& miniperlmain$(OBJ_EXT) $(LIBPERL) opmini$(OBJ_EXT)
	-@rm -f miniperl.xok
	$(CC) $(CLDFLAGS) $(NAMESPACEFLAGS) -o miniperl \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) $(LLIBPERL) $(libs)
	$(LDLIBPTH) ./miniperl -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	*)
		$spitshell >>Makefile <<'!NO!SUBS!'
miniperl$(EXE_EXT): $& miniperlmain$(OBJ_EXT) $(LIBPERL) opmini$(OBJ_EXT)
	-@rm -f miniperl.xok
	$(LDLIBPTH) $(CC) $(CLDFLAGS) -o miniperl \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) $(LLIBPERL) $(libs)
	$(LDLIBPTH) ./miniperl -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	esac

	$spitshell >>Makefile <<'!NO!SUBS!'

perl$(EXE_EXT): $& perlmain$(OBJ_EXT) $(LIBPERL) $(DYNALOADER) $(static_ext) ext.libs $(PERLEXPORT)
	-@rm -f miniperl.xok
	$(SHRPENV) $(LDLIBPTH) $(CC) -o perl$(PERL_SUFFIX) $(PERL_PROFILE_LDFLAGS) $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(DYNALOADER) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

# Purify/Quantify Perls.

pureperl$(EXE_EXT): $& perlmain$(OBJ_EXT) $(LIBPERL) $(DYNALOADER) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) purify $(CC) -o pureperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(DYNALOADER) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

purecovperl$(EXE_EXT): $& perlmain$(OBJ_EXT) $(LIBPERL) $(DYNALOADER) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) purecov $(CC) -o purecovperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(DYNALOADER) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

quantperl$(EXE_EXT): $& perlmain$(OBJ_EXT) $(LIBPERL) $(DYNALOADER) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) quantify $(CC) -o quantperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(DYNALOADER) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

# Valgrind perl (currently Linux only)

perl.valgrind.config: config.sh
	@echo "To build perl.valgrind you must Configure -Doptimize=-g -Uusemymalloc, checking..."
	@$(MAKE) perl.config.dashg
	@echo "Checking usemymalloc='n' in config.sh..."
	@grep "^usemymalloc="    config.sh
	@grep "^usemymalloc='n'" config.sh >/dev/null || exit 1
	@echo "And of course you have to have valgrind..."
	valgrind ./perl -e 1 2>/dev/null || exit 1

# Third Degree Perl (Tru64 only)

perl.config.dashg:
	@echo "Checking optimize='-g' in config.sh..."
	@grep "^optimize="     config.sh
	@grep "^optimize='-g'" config.sh >/dev/null || exit 1

perl.third.config: config.sh
	@echo "To build perl.third you must Configure -Doptimize=-g -Uusemymalloc, checking..."
	@$(MAKE) perl.config.dashg
	@echo "Checking usemymalloc='n' in config.sh..."
	@grep "^usemymalloc="    config.sh
	@grep "^usemymalloc='n'" config.sh >/dev/null || exit 1

perl.third: /usr/bin/atom perl.third.config perl
	atom -tool third -L. -all -gp -toolargs="-invalid -uninit heap+stack+copy -min 0" perl
	@echo "Now you may run perl.third and then study perl.3log."

# Pixie Perls (Tru64 and IRIX only)

perl.pixie.config: config.sh
	@echo "To build perl.pixie you must Configure -Doptimize=-g, checking..."
	@$(MAKE) perl.config.dashg

perl.pixie.atom: /usr/bin/atom perl
	atom -tool pixie -L. -all -toolargs="-quiet" perl

perl.pixie.irix: perl
	pixie perl

perl.pixie: /usr/bin/pixie perl.pixie.config perl
	if test -x /usr/bin/atom; then \
	  $(MAKE) perl.pixie.atom; \
	else \
	  $(MAKE) perl.pixie.irix; \
	fi
	@echo "Now you may run perl.pixie and then run pixie."

# Gprof Perl

perl.config.dashpg:
	@echo "Checking optimize='-pg' in config.sh..."
	@grep "^optimize="      config.sh
	@grep "^optimize='.*-pg.*'" config.sh >/dev/null || exit 1

perl.gprof.config: config.sh
	@echo "To build perl.gprof you must Configure -Doptimize=-pg, checking..."
	@$(MAKE) perl.config.dashpg

perl.gprof: /usr/bin/gprof perl.gprof.config
	@-rm -f perl
	$(MAKE) PERL_SUFFIX=.gprof PERL_PROFILE_LDFLAGS=-pg perl
	@echo "Now you may run perl.gprof and then run gprof perl.gprof."

# Gcov Perl

perl.config.gcov:
	@echo "To build perl.gcov you must use gcc 3.0 or newer, checking..."
	@echo "Checking gccversion in config.sh..."
	@grep "^gccversion="      config.sh
	@grep "^gccversion='[3-9]\." config.sh >/dev/null || exit 1
	@echo "To build perl.gcov you must Configure -Dccflags=-fprofile-arcs -ftest-coverage, checking..."
	@echo "Checking ccflags='-fprofile-arcs -ftest-coverage' in config.sh..."
	@grep "^ccflags="      config.sh
	@grep "^ccflags='.*-fprofile-arcs -ftest-coverage.*'" config.sh >/dev/null || exit 1

perl.gcov: perl.config.gcov
	@-rm -f perl
	$(MAKE) PERL_SUFFIX=.gcov PERL_PROFILE_LDFLAGS='' perl
	@echo "Now you may run perl.gcov and then run gcov some.c."

# Microperl.  This is just a convenience thing if one happens to
# build also the full Perl and therefore the real big Makefile:
# usually one should manually explicitly issue the below command.

.PHONY: microperl
microperl:
	$(MAKE) -f Makefile.micro

# This version, if specified in Configure, does ONLY those scripts which need
# set-id emulation.  Suidperl must be setuid root.  It contains the "taint"
# checks as well as the special code to validate that the script in question
# has been invoked correctly.

suidperl$(EXE_EXT): $& sperl$(OBJ_EXT) perlmain$(OBJ_EXT) $(LIBPERL) $(DYNALOADER) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) $(CC) -o suidperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) sperl$(OBJ_EXT) $(DYNALOADER) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

!NO!SUBS!

fi

$spitshell >>Makefile <<'!NO!SUBS!'

sperl$(OBJ_EXT): perl.c $(h)
	$(RMS) sperl.c
	$(LNS) perl.c sperl.c
	$(CCCMD) -DIAMSUID sperl.c
	$(RMS) sperl.c

# We have to call our ./makedir because Ultrix 4.3 make can't handle the line
#	test -d lib/auto || mkdir lib/auto
# We need to autosplit in two steps because VOS can't handle so many args
#
.PHONY: preplibrary
preplibrary: miniperl$(EXE_EXT) lib/Config.pm lib/lib.pm $(PREPLIBRARY_LIBPERL)
	@sh ./makedir lib/auto
	@echo "	AutoSplitting perl library"
	$(LDLIBPTH) ./miniperl -Ilib -e 'use AutoSplit; \
		autosplit_lib_modules(@ARGV)' lib/*.pm
	$(LDLIBPTH) ./miniperl -Ilib -e 'use AutoSplit; \
		autosplit_lib_modules(@ARGV)' lib/*/*.pm
	$(MAKE) lib/re.pm

# Take care to avoid modifying lib/Config.pm without reason
# (If trying to create a new port and having problems with the configpm script,
# try 'make minitest' and/or commenting out the tests at the end of configpm.)
lib/Config.pm: config.sh miniperl$(EXE_EXT) configpm Porting/Glossary
	$(LDLIBPTH) ./miniperl -Ilib configpm configpm.tmp
	sh mv-if-diff configpm.tmp $@

lib/ExtUtils/Miniperl.pm: miniperlmain.c miniperl$(EXE_EXT) minimod.pl lib/Config.pm
	$(LDLIBPTH) ./miniperl minimod.pl > minimod.tmp
	sh mv-if-diff minimod.tmp $@
	-touch lib/ExtUtils/Miniperl.pm

lib/re.pm: ext/re/re.pm
	cp ext/re/re.pm ext/re/re.tmp && sh mv-if-diff ext/re/re.tmp lib/re.pm

$(plextract):	miniperl$(EXE_EXT) lib/Config.pm x2p/s2p
	@-rm -f $@
	$(LDLIBPTH) ./miniperl -Ilib $@.PL

x2p/s2p: miniperl$(EXE_EXT) lib/Config.pm x2p/s2p.PL
	cd x2p; $(LDLIBPTH) $(MAKE) s2p

lib/lib.pm:	miniperl$(EXE_EXT) lib/Config.pm
	@-rm -f $@
	$(LDLIBPTH) ./miniperl -Ilib lib/lib_pm.PL

extra.pods: miniperl$(EXE_EXT)
	-@test -f extra.pods && rm -f `cat extra.pods`
	-@rm -f extra.pods
	-@for x in `grep -l '^=[a-z]' README.* | grep -v README.vms` ; do \
	    nx=`echo $$x | sed -e "s/README\.//"`; \
	    cd pod ; $(LNS) ../$$x "perl"$$nx".pod" ; cd .. ; \
	    echo "pod/perl"$$nx".pod" >> extra.pods ; \
	done
	-@rm -f pod/perlvms.pod
	-@test -f vms/perlvms.pod && cd pod && $(LNS) ../vms/perlvms.pod perlvms.pod && cd .. && echo "pod/perlvms.pod" >> extra.pods
	-@rm -f pod/perldelta.pod
	-@test -f pod/perl583delta.pod && cd pod && $(LNS) perl583delta.pod perldelta.pod && cd .. && echo "pod/perldelta.pod" >> extra.pods

extras.make: perl$(EXE_EXT)
	-@test -s extras.lst && PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(LDLIBPTH) ./perl -Ilib -MCPAN -e '@ARGV&&make(@ARGV)' `cat extras.lst`

extras.test: perl$(EXE_EXT)
	-@test -s extras.lst && PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(LDLIBPTH) ./perl -Ilib -MCPAN -e '@ARGV&&test(@ARGV)' `cat extras.lst`

extras.install: perl$(EXE_EXT)
	-@test -s extras.lst && PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(LDLIBPTH) ./perl -Ilib -MCPAN -e '@ARGV&&install(@ARGV)' `cat extras.lst`

.PHONY: install install-strip install-all install-verbose install-silent \
	no-install install.perl install.man install.html

META.yml:	Porting/makemeta Porting/Maintainers.pl Porting/Maintainers.pm
	$(LDLIBPTH) ./miniperl -Ilib Porting/makemeta

install-strip:
	$(MAKE) STRIPFLAGS=-s install DESTDIR="$(DESTDIR)"

install install-all:
	$(MAKE) install.perl install.man STRIPFLAGS=$(STRIPFLAGS) DESTDIR="$(DESTDIR)"

install-verbose:
	$(MAKE) install.perl install.man STRIPFLAGS=$(STRIPFLAGS) INSTALLFLAGS=-V DESTDIR="$(DESTDIR)"

install-silent:
	$(MAKE) install.perl install.man STRIPFLAGS=$(STRIPFLAGS) INSTALLFLAGS=-S DESTDIR="$(DESTDIR)"

no-install:
	$(MAKE) install.perl install.man STRIPFLAGS=$(STRIPFLAGS) INSTALLFLAGS=-n DESTDIR="$(DESTDIR)"

# Set this to an empty string to avoid an attempt of rebuild before install
INSTALL_DEPENDENCE = all

install.perl:	$(INSTALL_DEPENDENCE) installperl
	if [ -n "$(COMPILE)" ]; \
	then \
		cd utils; $(MAKE) compile; \
		cd ../x2p; $(MAKE) compile; \
		cd ../pod; $(MAKE) compile; \
	else :; \
	fi
	$(LDLIBPTH) ./perl installperl --destdir=$(DESTDIR) $(INSTALLFLAGS) $(STRIPFLAGS)
	$(MAKE) extras.install

install.man:	all installman
	$(LDLIBPTH) ./perl installman --destdir=$(DESTDIR) $(INSTALLFLAGS)

# XXX Experimental. Hardwired values, but useful for testing.
# Eventually Configure could ask for some of these values.
install.html: all installhtml
	-@test -f README.vms && cd vms && $(LNS) ../README.vms README_vms.pod && cd ..
	$(LDLIBPTH) ./perl installhtml                   \
      --podroot=. --podpath=. --recurse  \
      --htmldir=$(privlib)/html     \
      --htmlroot=$(privlib)/html    \
      --splithead=pod/perlipc     \
      --splititem=pod/perlfunc    \
      --libpods=perlfunc:perlguts:perlvar:perlrun:perlop \
      --verbose


# I now supply perly.c with the kits, so the following section is
# used only if you force byacc to run by saying
# 	make run_byacc
# Since we patch up the byacc output, the perly.fixer script needs
# to run with precisely the same version of byacc as I use.  You
# normally shouldn't remake perly.[ch].

.PHONY: check_byacc run_byacc

check_byacc:
	@$(BYACC) -V 2>&1 | grep 'version 1\.8\.2'

run_byacc:	FORCE check_byacc
	$(BYACC) -d perly.y
	-chmod 664 perly.c perly.h
	sh $(shellflags) ./perly.fixer y.tab.c perly.c
	sed -e 's/fprintf *( *stderr *,/PerlIO_printf(Perl_debug_log,/g' \
	    -e 's/y\.tab/perly/g' perly.c >perly.tmp && mv perly.tmp perly.c
	sed -e '/^extern YYSTYPE yy/D' y.tab.h >yh.tmp && mv yh.tmp y.tab.h
	cmp -s y.tab.h perly.h && rm -f y.tab.h || mv y.tab.h perly.h
	perl -i.old perlyline.pl perly.c
	chmod 664 vms/perly_c.vms vms/perly_h.vms
	perl vms/vms_yfix.pl perly.c perly.h vms/perly_c.vms vms/perly_h.vms

# We don't want to regenerate perly.c and perly.h, but they might
# appear out-of-date after a patch is applied or a new distribution is
# made.
perly.c: perly.y
	-@sh -c true

perly.h: perly.y
	-@sh -c true

PERLYVMS = vms/perly_c.vms vms/perly_h.vms

$(PERLYVMS):	perly.c perly.h vms/vms_yfix.pl
	perl vms/vms_yfix.pl perly.c perly.h vms/perly_c.vms vms/perly_h.vms

# No compat3.sym here since and including the 5.004_50.
# No interp.sym since 5.005_03.
SYM  = global.sym globvar.sym perlio.sym pp.sym

SYMH = perlvars.h intrpvar.h thrdvar.h

CHMOD_W = chmod +w

# The following files are generated automatically
#	autodoc.pl:	pod/perlapi.pod pod/perlintern.pod
#	bytecode.pl:	ext/ByteLoader/byterun.h ext/ByteLoader/byterun.c
#			ext/B/B/Asmdata.pm
#	embed.pl:	proto.h embed.h embedvar.h global.sym
#			perlapi.h perlapi.c 
# [* embed.pl needs pp.sym generated by opcode.pl! *]
#	keywords.pl:	keywords.h
#	opcode.pl:	opcode.h opnames.h pp_proto.h pp.sym
#	regcomp.pl:	regnodes.h
#	warnings.pl:	warnings.h lib/warnings.pm
# The correct versions should be already supplied with the perl kit,
# in case you don't have perl available.
# To force them to be regenerated, run
#       perl regen.pl
# with your existing copy of perl
# (make regen_headers is kept for backwards compatibility)

AUTOGEN_FILES = keywords.h opcode.h opnames.h pp_proto.h pp.sym proto.h \
		embed.h embedvar.h global.sym \
		pod/perlintern.pod pod/perlapi.pod \
		perlapi.h perlapi.c ext/ByteLoader/byterun.h \
		ext/ByteLoader/byterun.c ext/B/B/Asmdata.pm regnodes.h \
		warnings.h lib/warnings.pm

.PHONY: regen_headers regen_pods regen_all

regen regen_headers:	FORCE
	-perl regen.pl

regen_pods:	FORCE
	-cd pod; $(LDLIBPTH) $(MAKE) regen_pods

regen_all: $(PERLYVMS) regen regen_pods

.PHONY:	manisort manicheck

manisort:	FORCE
	LC_ALL=C sort -fdc MANIFEST || (echo "WARNING: re-sorting MANIFEST"; \
		LC_ALL=C sort -fdo MANIFEST MANIFEST)

manicheck:	FORCE
	perl Porting/manicheck

# Extensions:
# Names added to $(dynamic_ext) or $(static_ext) or $(nonxs_ext) will
# automatically get built.  There should ordinarily be no need to change
# any of this part of makefile.
#
# The dummy dependency is a place holder in case $(dynamic_ext) or
# $(static_ext) is empty.
#
# DynaLoader may be needed for extensions that use Makefile.PL.

$(DYNALOADER):	miniperl$(EXE_EXT) preplibrary FORCE
	@$(LDLIBPTH) sh ext/util/make_ext $(STATIC) $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL)

d_dummy $(dynamic_ext):	miniperl$(EXE_EXT) preplibrary $(DYNALOADER) FORCE
	@$(LDLIBPTH) sh ext/util/make_ext dynamic $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL)

s_dummy $(static_ext):	miniperl$(EXE_EXT) preplibrary $(DYNALOADER) FORCE
	@$(LDLIBPTH) sh ext/util/make_ext $(STATIC) $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL)

n_dummy $(nonxs_ext):	miniperl$(EXE_EXT) preplibrary $(DYNALOADER) FORCE
	@$(LDLIBPTH) sh ext/util/make_ext nonxs $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL)

.PHONY: clean _tidy _mopup _cleaner1 _cleaner2 \
	realclean _realcleaner clobber _clobber \
	distclean veryclean _verycleaner

clean:		_tidy _mopup

realclean:	_realcleaner _mopup
	@echo "Note that '$(MAKE) realclean' does not delete config.sh or Policy.sh"

_clobber:
	-@rm -f Cross/run-* Cross/to-* Cross/from-*
	rm -f config.sh cppstdin Policy.sh extras.lst

clobber:	_realcleaner _mopup _clobber

distclean:	clobber

# Like distclean but also removes emacs backups and *.orig.
veryclean:	_verycleaner _mopup _clobber
	-@rm -f Obsolete Wanted

# Do not 'make _mopup' directly.
_mopup:
	rm -f *$(OBJ_EXT) *$(LIB_EXT) all perlmain.c
	-rmdir .depending
	-@test -f extra.pods && rm -f `cat extra.pods`
	-@test -f vms/README_vms.pod && rm -f vms/README_vms.pod
	-rm -f perl.exp ext.libs extra.pods opmini.o
	-rm -f perl.export perl.dll perl.libexp perl.map perl.def
	-rm -f perl.loadmap miniperl.loadmap perl.prelmap miniperl.prelmap
	-rm -f perl.third lib*.so.perl.third perl.3log t/perl.third t/perl.3log
	-rm -f perl.pixie lib*.so.perl.pixie lib*.so.Addrs
	-rm -f perl.Addrs perl.Counts t/perl.Addrs t/perl.Counts *perl.xok
	-rm -f perlld cygwin.c ld2 libperl*.def libperl*.dll cygperl*.dll *.exe.stackdump
	-rm -f perl$(EXE_EXT) suidperl$(EXE_EXT) miniperl$(EXE_EXT) $(LIBPERL) libperl.* microperl
	-rm -f opcode.h-old opnames.h-old pp.sym-old pp_proto.h-old

# Do not 'make _tidy' directly.
_tidy:
	-cd pod; $(LDLIBPTH) $(MAKE) clean
	-cd utils; $(LDLIBPTH) $(MAKE) clean
	-cd x2p; $(LDLIBPTH) $(MAKE) clean
	-@for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) $(nonxs_ext) ; do \
	$(LDLIBPTH) sh ext/util/make_ext clean $$x MAKE=$(MAKE) ; \
	done
	rm -f testcompile compilelog

_cleaner1:
	-cd os2; rm -f Makefile
	-cd pod; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-cd utils; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-cd x2p; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-@for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) $(nonxs_ext) ; do \
	$(LDLIBPTH) sh ext/util/make_ext $(CLEAN) $$x MAKE=$(MAKE) ; \
	done

# Some systems do not support "?", so keep these files separate.
_cleaner2:
	-rm -f core.*perl.*.? t/core.perl.*.? .?*.c
	rm -f core *perl.core t/core t/*perl.core
	rm -f t/misctmp* t/forktmp* t/tmp* t/c t/perl$(EXE_EXT) t/rantests
	rm -f so_locations $(LIBPERL_NONSHR) $(MINIPERL_NONSHR)
	rm -rf $(addedbyconf)
	rm -f $(FIRSTMAKEFILE) $(FIRSTMAKEFILE).old makefile.old
	rm -f $(private) lib/Config.pod
	rm -rf lib/auto
	rm -f lib/.exists lib/*/.exists lib/*/*/.exists
	rm -f h2ph.man pstruct
	rm -rf .config
	rm -f preload
	rm -f testcompile compilelog
	-rmdir lib/B lib/Data lib/Digest
	rm -rf lib/Encode
	-rmdir lib/Filter/Util lib/IO/Socket lib/IO
	-rmdir lib/List lib/MIME lib/Scalar lib/Sys 
	-rmdir lib/threads lib/XS

_realcleaner:
	@$(LDLIBPTH) $(MAKE) _cleaner1 CLEAN=realclean
	@$(LDLIBPTH) $(MAKE) _cleaner2

_verycleaner:
	@$(LDLIBPTH) $(MAKE) _cleaner1 CLEAN=veryclean
	@$(LDLIBPTH) $(MAKE) _cleaner2
	-rm -f *~ *.orig */*~ */*.orig */*/*~ */*/*.orig

# The following lint has practically everything turned on.  Unfortunately,
# you have to wade through a lot of mumbo jumbo that can't be suppressed.
# If the source file has a /*NOSTRICT*/ somewhere, ignore the lint message
# for that spot.

.PHONY: lint
lint: $(c)
	lint $(lintflags) $(defs) perly.c $(c) > perl.fuzz

# Need to unset during recursion to go out of loop.
# The README below ensures that the dependency list is never empty and
# that when MAKEDEPEND is empty $(FIRSTMAKEFILE) doesn't need rebuilding.

MAKEDEPEND = Makefile makedepend

$(FIRSTMAKEFILE):	README $(MAKEDEPEND)
	$(MAKE) depend MAKEDEPEND=

config.h: config_h.SH config.sh
	$(SHELL) config_h.SH

# When done, touch perlmain.c so that it doesn't get remade each time.
.PHONY: depend
depend: makedepend
	sh ./makedepend MAKE=$(MAKE)
	- test -s perlmain.c && touch perlmain.c
	cd x2p; $(MAKE) depend

# Cannot postpone this until $firstmakefile is ready ;-)
makedepend: makedepend.SH config.sh
	sh ./makedepend.SH

.PHONY: test check test_prep test_prep_nodll test_prep_pre _test_prep \
	test_tty test-tty _test_tty test_notty test-notty _test_notty \
	utest ucheck test.utf8 check.utf8 test.torture torturetest \
	test.third check.third utest.third ucheck.third test_notty.third \
	test.deparse test_notty.deparse test_harness test_harness_notty \
	test.bytecompile minitest coretest test.taintwarn

# Cannot delegate rebuilding of t/perl to make
# to allow interlaced test and minitest

TESTFILE=TEST

_test_prep:
	cd t && (rm -f $(PERL)$(EXE_EXT); $(LNS) ../$(PERL)$(EXE_EXT) $(PERL)$(EXE_EXT))

# Architecture-neutral stuff:

test_prep_pre: preplibrary utilities $(nonxs_ext)

test_prep: test_prep_pre miniperl$(EXE_EXT) perl$(EXE_EXT) $(dynamic_ext) $(TEST_PERL_DLL)
	PERL=./perl $(MAKE) _test_prep

_test_tty:
	cd t && $(PERL_DEBUG) $(LDLIBPTH) $(PERL) $(TESTFILE) $(TEST_ARGS) </dev/tty

_test_notty:
	cd t && $(PERL_DEBUG) PERL_SKIP_TTY_TEST=1 $(LDLIBPTH) $(PERL) $(TESTFILE) $(TEST_ARGS)

# The second branch is for testing without a tty or controlling terminal,
# see t/op/stat.t
_test:
	if (true </dev/tty) >/dev/null 2>&1; then \
	  $(MAKE) TEST_ARGS=$(TEST_ARGS) TESTFILE=$(TESTFILE) _test_tty   ; \
	else \
	  $(MAKE) TEST_ARGS=$(TEST_ARGS) TESTFILE=$(TESTFILE) _test_notty ; \
	fi
	@echo "Ran tests" > t/rantests

test check: test_prep
	PERL=./perl $(MAKE) _test

test_tty: test_prep
	PERL=./perl $(MAKE) _test_tty

test_notty: test_prep
	PERL=./perl $(MAKE) _test_notty

utest ucheck test.utf8 check.utf8: test_prep
	PERL=./perl TEST_ARGS=-utf8 $(MAKE) _test

coretest: test_prep
	PERL=./perl TEST_ARGS=-core $(MAKE) _test

test-prep:	test_prep

test-tty:	test_tty

test-notty:	test_notty

# Torture testing

test.torture torturetest:	test_prep
	PERL=./perl TEST_ARGS=-torture $(MAKE) _test

# Targets for valgrind testing:

test_prep.valgrind: test_prep perl.valgrind
	PERL=./perl $(MAKE) _test_prep

test.valgrind check.valgrind:	test_prep perl.valgrind.config
	PERL=./perl PERL_DEBUG=PERL_VALGRIND=1 $(MAKE) _test

utest.valgrind ucheck.valgrind: test_prep.valgrind perl.valgrind
	PERL=./perl PERL_DEBUG=PERL_VALGRIND=1 TEST_ARGS=-utf8 $(MAKE) _test

test_notty.valgrind: test_prep.valgrind perl.valgrind
	PERL=./perl $(MAKE) PERL_DEBUG=PERL_VALGRIND=1 _test_notty

# Targets for Third Degree testing.

test_prep.third: test_prep perl.third
	PERL=./perl.third $(MAKE) _test_prep

test.third check.third:	test_prep.third perl.third
	PERL=./perl.third PERL_DEBUG=PERL_3LOG=1 $(MAKE) _test

utest.third ucheck.third: test_prep.third perl.third
	PERL=./perl.third PERL_DEBUG=PERL_3LOG=1 TEST_ARGS=-utf8 $(MAKE) _test

test_notty.third: test_prep.third perl.third
	PERL=./perl.third $(MAKE) PERL_DEBUG=PERL_3LOG=1 _test_notty

# Targets for Bytecode/ByteLoader testing.

test.bytecompile:	test_prep
	PERL=./perl TEST_ARGS=-bytecompile $(MAKE) _test

# Targets for Deparse testing.

test.deparse:	test_prep
	PERL=./perl TEST_ARGS=-deparse $(MAKE) _test

test_notty.deparse:	test_prep
	PERL=./perl TEST_ARGS=-deparse $(MAKE) _test_notty

# Targets to run the test suite with -t

test.taintwarn:	test_prep
	PERL=./perl TEST_ARGS=-taintwarn $(MAKE) _test

# Can't depend on lib/Config.pm because that might be where miniperl
# is crashing.
minitest: miniperl$(EXE_EXT) lib/re.pm
	-@test -f lib/lib.pm && test -f lib/Config.pm || \
	  $(MAKE) lib/Config.pm lib/lib.pm
	@echo " "
	@echo "You may see some irrelevant test failures if you have been unable"
	@echo "to build lib/Config.pm or lib/lib.pm."
	@echo " "
	- cd t && (rm -f perl$(EXE_EXT); $(LNS) ../miniperl$(EXE_EXT) perl$(EXE_EXT)) \
		&& $(LDLIBPTH) ./perl TEST base/*.t comp/*.t cmd/*.t run/*.t io/*.t op/*.t uni/*.t </dev/tty

# Test via harness

test_harness: test_prep
	PERL=./perl $(MAKE) TESTFILE=harness _test

test_harness_notty: test_prep
	PERL=./perl HARNESS_NOTTY=1 $(MAKE) TESTFILE=harness _test

# Handy way to run perlbug -ok without having to install and run the
# installed perlbug. We don't re-run the tests here - we trust the user.
# Please *don't* use this unless all tests pass.
# If you want to report test failures, use "make nok" instead.

.PHONY: ok okfile oknack okfilenack nok nokfile noknack nokfilenack

ok:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -ok -s '(UNINSTALLED)'

okfile:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -F perl.ok

oknack:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -A

okfilenack:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -F perl.ok -A

nok:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -nok -s '(UNINSTALLED)'

nokfile:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -F perl.nok

noknack:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -A

nokfilenack:	utilities
	$(LDLIBPTH) ./perl -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -F perl.nok -A

.PHONY: clist hlist shlist pllist

clist:	$(c)
	echo $(c) | tr ' ' $(TRNL) >.clist

hlist:  $(h)
	echo $(h) | tr ' ' $(TRNL) >.hlist

shlist: $(sh)
	echo $(sh) | tr ' ' $(TRNL) >.shlist

pllist: $(pl)
	echo $(pl) | tr ' ' $(TRNL) >.pllist

Makefile: Makefile.SH ./config.sh
	$(SHELL) Makefile.SH

.PHONY: distcheck
distcheck: FORCE
	perl '-MExtUtils::Manifest=&fullcheck' -e 'fullcheck()'

.PHONY: elc
elc:	emacs/cperl-mode.elc

emacs/cperl-mode.elc: emacs/cperl-mode.el
	-cd emacs; emacs -batch -q -no-site-file -f batch-byte-compile cperl-mode.el

.PHONY: etags ctags tags

etags:	TAGS

TAGS: emacs/cperl-mode.elc
	sh emacs/ptags

ctags:	tags

# Let's hope make will not go into an infinite loop on case-unsensitive systems
# This may also fail if . is in the head of the path, since perl will
# require -Ilib
tags:	TAGS
	perl emacs/e2ctags.pl TAGS > tags

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

# Now do any special processing required before building.

case "$ebcdic" in
define)
    xxx=''
    echo "This is an EBCDIC system, checking if any parser files need regenerating." >&2
case "$osname" in
os390|posix-bc)
    rm -f y.tab.c y.tab.h
    # yacc must be a reentrant ("pure") Bison in BS2000 Posix!
    yacc -d perly.y >/dev/null 2>&1
    if cmp -s y.tab.c perly.c; then
        rm -f y.tab.c
    else
        echo "perly.y -> perly.c" >&2
        mv -f y.tab.c perly.c
        chmod u+w perly.c
        sed -e '/^#include "perl\.h"/a\
\
#define yydebug    PL_yydebug\
#define yynerrs    PL_yynerrs\
#define yyerrflag  PL_yyerrflag\
#define yychar     PL_yychar\
#define yyval      PL_yyval\
#define yylval     PL_yylval'				\
            -e '/YYSTYPE *yyval;/D'			\
            -e '/YYSTYPE *yylval;/D'			\
            -e '/int  yychar,/,/yynerrs;/D'		\
            -e 's/int yydebug = 0;/yydebug = 0;/'	\
            -e 's/[^_]realloc(/PerlMem_realloc(/g'	\
            -e 's/fprintf *( *stderr *,/PerlIO_printf(Perl_debug_log,/g' \
            -e 's/y\.tab/perly/g' perly.c >perly.tmp && mv perly.tmp perly.c
        xxx="$xxx perly.c"
    fi
    case "$osname:$usethreads" in
    os390:define)
	sed -e 's@^extern int yychar, yyerrflag;@/* extern int yychar, yyerrflag; */@' perly.c > perly.tmp && mv perly.tmp perly.c
	;;
    esac
    if cmp -s y.tab.h perly.h; then
        rm -f y.tab.h
    else
        echo "perly.y -> perly.h" >&2
        mv -f y.tab.h perly.h
        xxx="$xxx perly.h"
    fi
    if cd x2p
    then
        rm -f y.tab.c y.tab.h
        case "$osname" in
        posix-bc)
           # we are using two different yaccs in BS2000 Posix!
           byacc a2p.y >/dev/null 2>&1
           ;;
        *) # e.g. os390
           yacc  a2p.y >/dev/null 2>&1
           ;;
        esac
        if cmp -s y.tab.c a2p.c
        then
            rm -f y.tab.c
        else
            echo "a2p.y -> a2p.c" >&2
            mv -f y.tab.c a2p.c
            chmod u+w a2p.c
            sed -e 's/fprintf *( *stderr *,/PerlIO_printf(Perl_debug_log,/g' \
		-e 's|^static void __YY_YACC_MAIN.*BS2000.*|/*static main deleted*/|' \
                -e 's/y\.tab/a2p/g' a2p.c >a2p.tmp && mv a2p.tmp a2p.c
            xxx="$xxx a2p.c"
        fi
        # In case somebody yacc -d:ed the a2p.y.
        if test -f y.tab.h
        then
            if cmp -s y.tab.h a2p.h
            then
                rm -f y.tab.h
            else
                echo "a2p.h -> a2p.h" >&2
                mv -f y.tab.h a2p.h
                xxx="$xxx a2p.h"
            fi
        fi
        cd ..
    fi
    ;;
vmesa)
    # Do nothing in VM/ESA.
    ;;
*)
    echo "'$osname' is an EBCDIC system I don't know that well." >&4
    ;;
esac
    case "$xxx" in
    '') echo "No parser files were regenerated.  That's okay." >&2 ;;
    esac
    ;;
esac


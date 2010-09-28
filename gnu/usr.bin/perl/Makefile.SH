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

case $CROSS_NAME in
'')
	Makefile=Makefile
	;;
*)
	# if cross-compilation, the Makefile named accordingly
	Makefile=Makefile-cross-$CROSS_NAME
	. Cross/config-${CROSS_NAME}.sh
	;;
esac

# H.Merijn Brand [17 Feb 2004]
# This comment is just to ensure that Configure will find variables that
# are removed/replaced in patches on blead, but are still needed in the
# 5.8.x, 5.6.x and 5.005.x maintainance tracks.
# metaconfig -m will scan all .SH files on this level (not deeper), and
# not in x2p and other subfolders. This file is as good as any .SH
# patch   references
# #22227 $baserev
# #22302 $yacc $byacc

# H.Merijn Brand [30 Oct 2004]
# Mentioned for the same reason for future reference
# #23434 $d_strlcat $d_strlcpy

: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

linklibperl='$(LIBPERL)'
linklibperl_nonshr=''
shrpldflags='$(LDDLFLAGS)'
ldlibpth=''
DPERL_EXTERNAL_GLOB='-DPERL_EXTERNAL_GLOB'
DPERL_IS_MINIPERL='-DPERL_IS_MINIPERL'
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
	static_ldflags=''
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
		shrpldflags="$shrpldflags -Wl,--out-implib=libperl.dll.a -Wl,--image-base,0x52000000"
		linklibperl="-L. -lperl"
		;;
	sunos*)
		linklibperl="-lperl"
		;;
	netbsd*|freebsd[234]*|openbsd*|dragonfly*)
		linklibperl="-L. -lperl"
		;;
	interix*)
		linklibperl="-L. -lperl"
		shrpldflags="$shrpldflags -Wl,--image-base,0x57000000"
		;;
	aix*)
		case "$cc" in
		gcc*)
			shrpldflags="-shared -Wl,-H512 -Wl,-T512 -Wl,-bhalt:4 -Wl,-bM:SRE -Wl,-bE:perl.exp"
			case "$osvers" in
			3*)	shrpldflags="$shrpldflags -e _nostart"
				;;
			*)	shrpldflags="$shrpldflags -Wl,-bnoentry"
				;;
			esac
			shrpldflags="$shrpldflags $ldflags $perllibs $cryptlib"
			linklibperl="-L $archlibexp/CORE -L `pwd | sed 's/\/UU$//'` -lperl"
			linklibperl_nonshr='-lperl_nonshr'
			;;
		*)
			shrpldflags="-H512 -T512 -bhalt:4 -bM:SRE -bE:perl.exp"
			case "$osvers" in
			3*)	shrpldflags="$shrpldflags -e _nostart"
				;;
			*)	shrpldflags="$shrpldflags -b noentry"
				;;
			esac
			shrpldflags="$shrpldflags $ldflags $perllibs $cryptlib"
			linklibperl="-L $archlibexp/CORE -L `pwd | sed 's/\/UU$//'` -lperl"
			linklibperl_nonshr='-lperl_nonshr'
			;;
		esac
		;;
	hpux*)
		linklibperl="-L `pwd | sed 's/\/UU$//'` -Wl,+s -Wl,+b$archlibexp/CORE -lperl"
		;;
	os390*)
            shrpldflags='-W l,XPLINK,dll'
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
	static_ldflags='CCCDLFLAGS='
	;;
esac

: Prepare dependency lists for Makefile.
dynamic_list=' '
dynamic_ext_re="lib/auto/re/re.$dlext"
extra_dep=''
for f in $dynamic_ext; do
    : the dependency named here will never exist
      base=`echo "$f" | sed 's/.*\///'`
    this_target="lib/auto/$f/$base.$dlext"  
    dynamic_list="$dynamic_list $this_target"

    : Parallel makes reveal that we have some interdependencies
    case $f in
	Encode|POSIX|SDBM_File) extra_dep="$extra_dep
$this_target: lib/auto/Cwd/Cwd.$dlext" ;;
	Math/BigInt/FastCalc|Devel/NYTProf) extra_dep="$extra_dep
$this_target: lib/auto/List/Util/Util.$dlext" ;;
	Unicode/Normalize) extra_dep="$extra_dep
$this_target: uni.data" ;;
	Text/ParseWords) extra_dep="$extra_dep
$this_target: lib/auto/Scalar/Util.$dlext" ;;
    esac
done

static_list=' '
for f in $static_ext; do
	base=`echo "$f" | sed 's/.*\///'`
	static_list="$static_list lib/auto/$f/$base\$(LIB_EXT)"
    : Parallel makes reveal that we have some interdependencies
    this_target="lib/auto/$f/$base\$(LIB_EXT)"
    case $f in
	Math/BigInt/FastCalc|Devel/NYTProf) extra_dep="$extra_dep
$this_target: lib/auto/List/Util/Util\$(LIB_EXT)" ;;
	Unicode/Normalize) extra_dep="$extra_dep
$this_target: uni.data" ;;
    esac
done

nonxs_list=' '
for f in $nonxs_ext; do
    p=`echo "$f" | tr / -`
    for d in ext dist cpan; do
	if test -d $d/$p; then
	    nonxs_list="$nonxs_list $d/$p/pm_to_blib"
	fi
    done
done

dtrace_h=''
dtrace_o=''
case "$usedtrace" in
define|true)
	dtrace_h='perldtrace.h' 
	$dtrace -G -s perldtrace.d -o perldtrace.tmp >/dev/null 2>&1 \
		&& rm -f perldtrace.tmp && dtrace_o='perldtrace$(OBJ_EXT)'
	;;
esac

echo "Extracting $Makefile (with variable substitutions)"
$spitshell >$Makefile <<!GROK!THIS!
# $Makefile
# This file is derived from Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.
#  Makefile is used to generate $firstmakefile.  The only difference
#  is that $firstmakefile has the dependencies filled in at the end.

CC = $cc
LD = $ld

LDFLAGS = $ldflags
CLDFLAGS = $ldflags

mallocsrc = $mallocsrc
mallocobj = $mallocobj
madlysrc = $madlysrc
madlyobj = $madlyobj
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
LLIBPERL_NONSHR= $linklibperl_nonshr
SHRPENV = $shrpenv

# Static targets are ordinarily built without CCCDLFLAGS.  However,
# if building a shared libperl.so that might later be linked into
# another application, then it might be appropriate to also build static
# extensions (usually just DynaLoader) with relocatable code (e.g. -fPIC
# for GNU cc).
STATIC_LDFLAGS = $static_ldflags

# The following is used to include the current directory in
# the dynamic loader path you are building a shared libperl.
LDLIBPTH = $ldlibpth

# Sometimes running an executable is an adventure.
RUN = $run

# These variables may need to be manually set for non-Unix systems.
AR = $full_ar
HOST_EXE_EXT = 
EXE_EXT = $_exe
LIB_EXT = $_a
OBJ_EXT = $_o
PATH_SEP = $p_

# Macros to invoke a copy of miniperl during the build.  Targets which
# are built using these macros should depend on \$(MINIPERL_EXE)
MINIPERL_EXE = miniperl\$(EXE_EXT)
MINIPERL = \$(LDLIBPTH) \$(RUN) ./miniperl\$(EXE_EXT) -Ilib

# Macros to invoke a copy of our fully operational perl during the build.
PERL_EXE = perl\$(EXE_EXT)
RUN_PERL = \$(LDLIBPTH) \$(RUN) ./perl\$(EXE_EXT)

# Macros to run our tests
RUN_TESTS = \$(LDLIBPTH) ./runtests

dynamic_ext = $dynamic_list
dynamic_ext_re = $dynamic_ext_re
static_ext = $static_list
nonxs_ext = $nonxs_list
ext = \$(dynamic_ext) \$(static_ext) \$(nonxs_ext)
DYNALOADER = DynaLoader\$(OBJ_EXT)

libs = $perllibs $cryptlib

public = \$(PERL_EXE) utilities translators

shellflags = $shellflags

# This is set to  MAKE=$make if your $make command doesn't
# do it for you.
$make_set_make

# Mention $gmake here so it gets probed for by Configure.

# If you're going to use valgrind and it can't be invoked as plain valgrind
# then you'll need to change this, or override it on the make command line.
VALGRIND=valgrind

DTRACE = $dtrace
DTRACE_H = $dtrace_h
DTRACE_O = $dtrace_o

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
$spitshell >>$Makefile <<EOF

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

case $CROSS_NAME in
'')
## In the following dollars and backticks do not need the extra backslash.
$spitshell >>$Makefile <<'!NO!SUBS!'

CCCMD    = `sh $(shellflags) cflags "optimize='$(OPTIMIZE)'" $@`

CCCMDSRC = `sh $(shellflags) cflags "optimize='$(OPTIMIZE)'" $<`

CONFIGPM_FROM_CONFIG_SH = lib/Config.pm lib/Config_heavy.pl
CONFIGPM = $(CONFIGPM_FROM_CONFIG_SH)

CONFIGPOD = lib/Config.pod

CONFIGH = config.h
!NO!SUBS!
	;;
*)
	# if cross-compilation
$spitshell >>$Makefile <<!GROK!THIS!
CROSS_NAME = $CROSS_NAME
CROSS_LIB = xlib/$CROSS_NAME

CCCMD    = \`sh \$(shellflags) cflags-cross-$CROSS_NAME "optimize='\$(OPTIMIZE)'" \$@\` -I\$(CROSS_LIB)
CCCMDSRC = \`sh \$(shellflags) cflags-cross-$CROSS_NAME "optimize='\$(OPTIMIZE)'" \$<\` -I\$(CROSS_LIB)
CONFIGPM = xlib/\$(CROSS_NAME)/Config.pm
CONFIGPOD = xlib/\$(CROSS_NAME)/Config.pod
CONFIGH = xconfig.h

xconfig.h: config_h.SH Cross/config-\$(CROSS_NAME).sh
	CONFIG_SH=Cross/config-\$(CROSS_NAME).sh CONFIG_H=xconfig.h \$(SHELL) config_h.SH
	#TODO \$(LDLIBPTH) ./miniperl$(EXE_EXT) -Ilib -MCross=\$(CROSS_NAME) config_h.PL "INST_VER=\$(INST_VER)" "CORE_DIR=\$(CROSS_LIB)" "CONFIG_H=xconfig.h"
	cp xconfig.h \$(CROSS_LIB)/
	cp xconfig.h \$(CROSS_LIB)/config.h

!GROK!THIS!
	;;
esac

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>$Makefile <<'!NO!SUBS!'

private = preplibrary $(CONFIGPM) $(CONFIGPOD) lib/ExtUtils/Miniperl.pm

# Files to be built with variable substitution before miniperl
# is available.
sh = Makefile.SH cflags.SH config_h.SH makeaperl.SH makedepend.SH \
	myconfig.SH writemain.SH pod/Makefile.SH

shextract = Makefile cflags config.h makeaperl makedepend \
	makedir myconfig writemain pod/Makefile

# Files to be built with variable substitution after miniperl is
# available.  Dependencies handled manually below (for now).

pl = pod/pod2html.PL pod/pod2latex.PL pod/pod2man.PL pod/pod2text.PL \
	pod/pod2usage.PL pod/podchecker.PL pod/podselect.PL

plextract = pod/pod2html pod/pod2latex pod/pod2man pod/pod2text \
	pod/pod2usage pod/podchecker pod/podselect

addedbyconf = UU $(shextract) $(plextract) pstruct

# Unicode data files generated by mktables
unidatafiles = lib/unicore/Decomposition.pl lib/unicore/TestProp.pl \
	lib/unicore/CombiningClass.pl lib/unicore/Name.pl \
	lib/unicore/Heavy.pl lib/unicore/mktables.lst

# Directories of Unicode data files generated by mktables
unidatadirs = lib/unicore/To lib/unicore/lib

h1 = EXTERN.h INTERN.h XSUB.h av.h $(CONFIGH) cop.h cv.h dosish.h
h2 = embed.h form.h gv.h handy.h hv.h keywords.h mg.h op.h opcode.h
h3 = pad.h patchlevel.h perl.h perlapi.h perly.h pp.h proto.h regcomp.h
h4 = regexp.h scope.h sv.h unixish.h util.h iperlsys.h thread.h
h5 = utf8.h warnings.h mydtrace.h
h = $(h1) $(h2) $(h3) $(h4) $(h5)

c1 = av.c scope.c op.c doop.c doio.c dump.c gv.c hv.c mg.c reentr.c mro.c perl.c
c2 = perly.c pp.c pp_hot.c pp_ctl.c pp_sys.c regcomp.c regexec.c utf8.c sv.c
c3 = taint.c toke.c util.c deb.c run.c universal.c pad.c globals.c
c4 = perlio.c perlapi.c numeric.c mathoms.c locale.c pp_pack.c pp_sort.c
c5 = $(madlysrc) $(mallocsrc)

c = $(c1) $(c2) $(c3) $(c4) $(c5) miniperlmain.c perlmain.c opmini.c perlmini.c

obj0 = op$(OBJ_EXT) perl$(OBJ_EXT)
obj1 = $(madlyobj) $(mallocobj) gv$(OBJ_EXT) toke$(OBJ_EXT) perly$(OBJ_EXT) pad$(OBJ_EXT) regcomp$(OBJ_EXT) dump$(OBJ_EXT) util$(OBJ_EXT) mg$(OBJ_EXT) reentr$(OBJ_EXT) mro$(OBJ_EXT)
obj2 = hv$(OBJ_EXT) av$(OBJ_EXT) run$(OBJ_EXT) pp_hot$(OBJ_EXT) sv$(OBJ_EXT) pp$(OBJ_EXT) scope$(OBJ_EXT) pp_ctl$(OBJ_EXT) pp_sys$(OBJ_EXT)
obj3 = doop$(OBJ_EXT) doio$(OBJ_EXT) regexec$(OBJ_EXT) utf8$(OBJ_EXT) taint$(OBJ_EXT) deb$(OBJ_EXT) universal$(OBJ_EXT) globals$(OBJ_EXT) perlio$(OBJ_EXT) perlapi$(OBJ_EXT) numeric$(OBJ_EXT) mathoms$(OBJ_EXT) locale$(OBJ_EXT) pp_pack$(OBJ_EXT) pp_sort$(OBJ_EXT)

mini_obj =  $(obj1) $(obj2) $(obj3) $(ARCHOBJS) $(DTRACE_O)
ndt_obj = $(obj0) $(obj1) $(obj2) $(obj3) $(ARCHOBJS)
obj = $(ndt_obj) $(DTRACE_O)

perltoc_pod_prereqs = extra.pods pod/perlapi.pod pod/perldelta.pod pod/perlintern.pod pod/perlmodlib.pod pod/perluniprops.pod
generated_pods = pod/perltoc.pod $(perltoc_pod_prereqs)

Icwd = -Icpan/Cwd -Icpan/Cwd/lib

lintflags = \
    -b \
    -n \
    -p \
    -Ncheck=%all \
    -Nlevel=4 \
    -errchk=parentheses \
    -errhdr=%all \
    -errfmt=src \
    -errtags \
    -erroff=E_ASSIGN_NARROW_CONV \
    -erroff=E_BAD_PTR_CAST \
    -erroff=E_BAD_PTR_CAST_ALIGN \
    -erroff=E_BAD_PTR_INT_COMBINATION \
    -erroff=E_BAD_SIGN_EXTEND \
    -erroff=E_BLOCK_DECL_UNUSED \
    -erroff=E_CASE_FALLTHRU \
    -erroff=E_CONST_EXPR \
    -erroff=E_CONSTANT_CONDITION \
    -erroff=E_END_OF_LOOP_CODE_NOT_REACHED \
    -erroff=E_EQUALITY_NOT_ASSIGNMENT \
    -erroff=E_EXPR_NULL_EFFECT \
    -erroff=E_FALSE_LOGICAL_EXPR \
    -erroff=E_INCL_NUSD \
    -erroff=E_LOOP_EMPTY \
    -erroff=E_MAIN_PARAM \
    -erroff=E_POINTER_TO_OBJECT \
    -erroff=E_PTRDIFF_OVERFLOW \
    -erroff=E_SHIFT_CNT_NEG_TOO_BIG_L \
    -erroff=E_STATIC_UNUSED \
    -erroff=E_TRUE_LOGICAL_EXPR

splintflags = \
    -I/usr/lib/gcc/i486-linux-gnu/4.0.2/include/ \
    -D__builtin_va_list=va_list \
    -Dsigjmp_buf=jmp_buf \
    -warnposix \
    \
    +boolint \
    +charintliteral \
    -fixedformalarray \
    -mustfreefresh \
    -nestedextern \
    -predboolint \
    -predboolothers \
    -preproc \
    -boolops \
    -shadow \
    -nullstate \
    +longintegral \
    +matchanyintegral \
    -type \
    \
    +line-len 999 \
    +weak

splintfiles = $(c1)

.c$(OBJ_EXT): 
	$(CCCMD) $(PLDLFLAGS) $*.c

.c.i: 
	$(CCCMDSRC) -E $*.c > $*.i

.c.s:
	$(CCCMDSRC) -S $*.c

all: $(FIRSTMAKEFILE) $(MINIPERL_EXE) miniperl $(generated_pods) $(private) $(unidatafiles) $(public) $(dynamic_ext) $(nonxs_ext) extras.make
	@echo " ";
	@echo "	Everything is up to date. Type '$(MAKE) test' to run test suite."

.PHONY: all translators utilities

# OpenBSD uses pre-generated lib/Config_git.pl and git_version.h files
# Both git_version.h and lib/Config_git.pl are built
# by make_patchnum.pl.
#git_version.h: lib/Config_git.pl
#
#lib/Config_git.pl: $(MINIPERL_EXE) make_patchnum.pl
#	$(MINIPERL) make_patchnum.pl

# make sure that we recompile perl.c if the git version changes
perl$(OBJ_EXT): git_version.h

translators:	$(MINIPERL_EXE) $(CONFIGPM) $(dynamic_ext) FORCE
	@echo " "; echo "	Making x2p stuff"; cd x2p; $(LDLIBPTH) $(MAKE) all

utilities:	$(MINIPERL_EXE) $(CONFIGPM) $(plextract) FORCE
	@echo " "; echo "	Making utilities"; cd utils; $(LDLIBPTH) $(MAKE) all


# This is now done by installman only if you actually want the man pages.
#	@echo " "; echo "	Making docs"; cd pod; $(MAKE) all;

# Phony target to force checking subdirectories.
# Apparently some makes require an action for the FORCE target.
.PHONY: FORCE
FORCE:
	@sh -c true
!NO!SUBS!
$spitshell >>$Makefile <<!GROK!THIS!

# We do a copy of the op.c instead of a symlink because gcc gets huffy
# if we have a symlink forest to another disk (it complains about too many
# levels of symbolic links, even if we have only two)

opmini.c: op.c
	\$(RMS) opmini.c
	\$(CPS) op.c opmini.c

opmini\$(OBJ_EXT): opmini.c
	\$(CCCMD) \$(PLDLFLAGS) $DPERL_EXTERNAL_GLOB opmini.c

perlmini.c: perl.c
	\$(RMS) perlmini.c
	\$(CPS) perl.c perlmini.c

perlmini\$(OBJ_EXT): perlmini.c
	\$(CCCMD) \$(PLDLFLAGS) $DPERL_IS_MINIPERL perlmini.c

globals\$(OBJ_EXT): uudmap.h bitcount.h

uudmap.h: bitcount.h

bitcount.h: generate_uudmap\$(HOST_EXE_EXT)
	\$(RUN) ./generate_uudmap\$(HOST_EXE_EXT) uudmap.h bitcount.h

generate_uudmap\$(HOST_EXE_EXT): generate_uudmap\$(OBJ_EXT)
	\$(CC) -o generate_uudmap\$(EXE_EXT) \$(LDFLAGS) generate_uudmap\$(OBJ_EXT) \$(libs)

!GROK!THIS!
$spitshell >>$Makefile <<'!NO!SUBS!'
miniperlmain$(OBJ_EXT): miniperlmain.c patchlevel.h
	$(CCCMD) $(PLDLFLAGS) $*.c

perlmain.c: miniperlmain.c config.sh $(FIRSTMAKEFILE)
	sh writemain $(DYNALOADER) $(static_ext) > perlmain.c

perlmain$(OBJ_EXT): perlmain.c
	$(CCCMD) $(PLDLFLAGS) $*.c

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
	$spitshell >>$Makefile <<!GROK!THIS!
LIBS			= $perllibs
# In AIX we need to change this for building Perl itself from
# its earlier definition (which is for building external
# extensions *after* Perl has been built and installed)
CCDLFLAGS		= `echo $ccdlflags|sed -e 's@-bE:.*/perl\.exp@-bE:perl.exp@'`

!GROK!THIS!
	case "$useshrplib" in
	define|true|[yY]*)
		$spitshell >>$Makefile <<'!NO!SUBS!'

LIBPERL_NONSHR		= libperl_nonshr$(LIB_EXT)
MINIPERL_NONSHR		= miniperl_nonshr$(EXE_EXT)

#$(LIBPERL_NONSHR): $(obj)
#	$(RMS) $(LIBPERL_NONSHR)
#	$(AR) rcu $(LIBPERL_NONSHR) $(obj)

$(MINIPERL_NONSHR): $(LIBPERL_NONSHR) miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT)  perlmini$(OBJ_EXT)
	$(CC) $(LDFLAGS) -o $(MINIPERL_NONSHR) miniperlmain$(OBJ_EXT) \
	    opmini$(OBJ_EXT) perlmini$(OBJ_EXT) $(LIBPERL_NONSHR) $(LIBS)

MINIPERLEXP		= $(MINIPERL_NONSHR)

LIBPERLEXPORT		= perl.exp

!NO!SUBS!
		
		;;
	*)	
		$spitshell >>$Makefile <<'!NO!SUBS!'
MINIPERLEXP		= $(MINIPERL_EXE)

PERLEXPORT		= perl.exp

!NO!SUBS!
	;;
	esac
	$spitshell >>$Makefile <<'!NO!SUBS!'
perl.exp: $(MINIPERLEXP) makedef.pl config.sh $(SYM) $(SYMH)
	./$(MINIPERLEXP) makedef.pl PLATFORM=aix CC_FLAGS="$(OPTIMIZE)" | sort -u | sort -f > perl.exp

!NO!SUBS!
	;;
os2)
	$spitshell >>$Makefile <<'!NO!SUBS!'
MINIPERLEXP		= miniperl

perl5.def: $(MINIPERLEXP) makedef.pl config.sh $(SYM) $(SYMH) miniperl.map
	./$(MINIPERLEXP) makedef.pl PLATFORM=os2 -DPERL_DLL=$(PERL_DLL) CC_FLAGS="$(OPTIMIZE)" > perl5.def

!NO!SUBS!
	;;
cygwin)
	$spitshell >>$Makefile <<'!NO!SUBS!'
cygwin.c: cygwin/cygwin.c
	$(LNS) cygwin/cygwin.c

LIBPERL_NONSHR		= libperl$(LIB_EXT)

$(LIBPERL_NONSHR): $(obj)
	$(RMS) $(LIBPERL_NONSHR)
	$(AR) rcu $(LIBPERL_NONSHR) $(obj)

!NO!SUBS!
	;;
esac

if test -s $Makefile_s ; then
	. $Makefile_s
	$spitshell >>$Makefile <<!GROK!THIS!

Makefile: $Makefile_s
!GROK!THIS!
else
	case "$dtrace_h" in
	?*)
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(DTRACE_H): perldtrace.d
	$(DTRACE) -h -s perldtrace.d -o $(DTRACE_H)

mydtrace.h: $(DTRACE_H)

!NO!SUBS!
		;;
	esac
	case "$dtrace_o" in
	?*)
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(DTRACE_O): perldtrace.d
	$(DTRACE) -G -s perldtrace.d -o $(DTRACE_O) $(ndt_obj)

!NO!SUBS!
		;;
    esac
	$spitshell >>$Makefile <<'!NO!SUBS!'
#$(LIBPERL): $& $(obj) $(DYNALOADER) $(LIBPERLEXPORT)
#!NO!SUBS!
#	case "$useshrplib" in
#	true)
#		$spitshell >>$Makefile <<'!NO!SUBS!'
#	rm -f $@
#	$(LD) -o $@ $(SHRPLDFLAGS) $(obj) $(DYNALOADER) $(libs)
#!NO!SUBS!
#		case "$osname" in
#		aix)
#			$spitshell >>$Makefile <<'!NO!SUBS!'
#	rm -f libperl$(OBJ_EXT)
#	mv $@ libperl$(OBJ_EXT)
#	$(AR) qv $(LIBPERL) libperl$(OBJ_EXT)
#!NO!SUBS!
#			;;
#		esac
#		;;
#	*)
#		$spitshell >>$Makefile <<'!NO!SUBS!'
#	rm -f $(LIBPERL)
#	$(AR) rcu $(LIBPERL) $(obj) $(DYNALOADER)
#	@$(ranlib) $(LIBPERL)
#!NO!SUBS!
#		;;
#	esac
#	$spitshell >>$Makefile <<'!NO!SUBS!'

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
	aix*|beos*)
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(MINIPERL_EXE): $& miniperlmain$(OBJ_EXT) $(mini_obj) opmini$(OBJ_EXT) perlmini$(OBJ_EXT)
	$(CC) -o $(MINIPERL_EXE) $(CLDFLAGS) \
	    $(mini_obj) \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perlmini$(OBJ_EXT) $(libs)
	$(LDLIBPTH) $(RUN) ./miniperl$(HOST_EXE_EXT) -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	next4*)
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(MINIPERL_EXE): $& miniperlmain$(OBJ_EXT) $(mini_obj) perlmini$(OBJ_EXT) opmini$(OBJ_EXT)
	$(CC) -o $(MINIPERL_EXE) $(mini_obj) \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perlmini$(OBJ_EXT) $(libs)
	$(LDLIBPTH) $(RUN) ./miniperl$(HOST_EXE_EXT) -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	darwin*)
		case "$osvers" in
		[1-6].*) ;;
		*) case "$ldflags" in
		    *"-flat_namespace"*) ;;
		    *) # to allow opmini.o to override stuff in libperl.dylib
		$spitshell >>$Makefile <<!NO!SUBS!
NAMESPACEFLAGS = -force_flat_namespace
!NO!SUBS!
		       ;;
		    esac
		    ;;
		esac
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(MINIPERL_EXE): $& miniperlmain$(OBJ_EXT) $(mini_obj) opmini$(OBJ_EXT) perlmini$(OBJ_EXT)
	-@rm -f miniperl.xok
	$(CC) $(CLDFLAGS) $(NAMESPACEFLAGS) -o $(MINIPERL_EXE) \
	    $(mini_obj) \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perlmini$(OBJ_EXT) $(libs)
	$(LDLIBPTH) $(RUN) ./miniperl$(HOST_EXE_EXT) -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	*)
		$spitshell >>$Makefile <<'!NO!SUBS!'
$(MINIPERL_EXE): $& miniperlmain$(OBJ_EXT) $(mini_obj) opmini$(OBJ_EXT) perlmini$(OBJ_EXT)
	-@rm -f miniperl.xok
	$(LDLIBPTH) $(CC) $(CLDFLAGS) -o $(MINIPERL_EXE) \
	    $(mini_obj) \
	    miniperlmain$(OBJ_EXT) opmini$(OBJ_EXT) perlmini$(OBJ_EXT) $(libs)
	$(LDLIBPTH) $(RUN) ./miniperl$(HOST_EXE_EXT) -w -Ilib -MExporter -e '<?>' || $(MAKE) minitest
!NO!SUBS!
		;;
	esac

	$spitshell >>$Makefile <<'!NO!SUBS!'

$(PERL_EXE): $& perlmain$(OBJ_EXT) $(LIBPERL) $(static_ext) ext.libs $(PERLEXPORT)
	-@rm -f miniperl.xok
	$(SHRPENV) $(LDLIBPTH) $(CC) -o perl$(PERL_SUFFIX) $(PERL_PROFILE_LDFLAGS) $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

# Purify/Quantify Perls.

pure$(PERL_EXE): $& perlmain$(OBJ_EXT) $(LIBPERL) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) purify $(CC) -o pureperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

purecov$(PERL_EXE): $& perlmain$(OBJ_EXT) $(LIBPERL) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) purecov $(CC) -o purecovperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

quant$(PERL_EXE): $& perlmain$(OBJ_EXT) $(LIBPERL) $(static_ext) ext.libs $(PERLEXPORT)
	$(SHRPENV) $(LDLIBPTH) quantify $(CC) -o quantperl $(CLDFLAGS) $(CCDLFLAGS) perlmain$(OBJ_EXT) $(static_ext) $(LLIBPERL) `cat ext.libs` $(libs)

# Valgrind perl (currently Linux only)

perl.valgrind.config: config.sh
	@echo "To build perl.valgrind you must Configure -Doptimize=-g -Uusemymalloc, checking..."
	@$(MAKE) perl.config.dashg
	@echo "Checking usemymalloc='n' in config.sh..."
	@grep "^usemymalloc="    config.sh
	@grep "^usemymalloc='n'" config.sh >/dev/null || exit 1
	@echo "And of course you have to have valgrind..."
	$(VALGRIND) ./perl -e 1 2>/dev/null || exit 1

# Third Degree Perl (Tru64 only)

perl.config.dashg:
	@echo "Checking optimize='-g' in config.sh..."
	@grep "^optimize=" config.sh
	@egrep "^optimize='(.*-g.*)'" config.sh >/dev/null || exit 1

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

!NO!SUBS!

fi

# Some environment have no system(), which mkpport uses.
# Let's try running the commands with shell.
case "${osname}" in
catamount)
$spitshell >>$Makefile <<!GROK!THIS!
.PHONY: makeppport
makeppport: \$(MINIPERL_EXE) \$(CONFIGPM)
	-@for f in Makefile.PL PPPort_pm.PL PPPort_xs.PL ppport_h.PL; do \
	(cd ext/Devel-PPPort && `pwd`/run.sh ../../$(MINIPERL_EXE) -I../../lib \$\$f); \
	done

!GROK!THIS!
;;
*)
$spitshell >>$Makefile <<'!NO!SUBS!'
.PHONY: makeppport
makeppport: $(MINIPERL_EXE) $(CONFIGPM) $(nonxs_ext)
	$(MINIPERL) $(Icwd) mkppport

!NO!SUBS!
;;
esac

$spitshell >>$Makefile <<'!NO!SUBS!'

.PHONY: preplibrary
preplibrary: $(MINIPERL_EXE) $(CONFIGPM) lib/re.pm $(PREPLIBRARY_LIBPERL)

$(CONFIGPM_FROM_CONFIG_SH): $(CONFIGPOD)

$(CONFIGPOD): config.sh $(MINIPERL_EXE) configpm Porting/Glossary lib/Config_git.pl
	$(MINIPERL) configpm

lib/ExtUtils/Miniperl.pm: miniperlmain.c $(MINIPERL_EXE) minimod.pl $(CONFIGPM)
	$(MINIPERL) minimod.pl > lib/ExtUtils/Miniperl.pm

lib/re.pm: ext/re/re.pm
	@-rm -f $@
	cp ext/re/re.pm lib/re.pm

$(plextract):	$(MINIPERL_EXE) $(CONFIGPM) x2p/s2p $(dynamic_ext)
	@-rm -f $@
	$(MINIPERL) $@.PL

x2p/s2p: $(MINIPERL_EXE) $(CONFIGPM) $(dynamic_ext) x2p/s2p.PL
	cd x2p; $(LDLIBPTH) $(MAKE) s2p

unidatafiles $(unidatafiles) pod/perluniprops.pod: uni.data

uni.data: $(MINIPERL_EXE) $(CONFIGPM) lib/unicore/mktables $(nonxs_ext)
#	$(MINIPERL) $(Icwd) lib/unicore/mktables -C lib/unicore -P pod -maketest -makelist -p
	touch uni.data

# $(PERL_EXE) and ext because buildtoc uses Text::Wrap uses re
# But also this ensures that all extensions are built before we try to scan
# them, which picks up Devel::PPPort's documentation.
pod/perltoc.pod: $(perltoc_pod_prereqs)  $(PERL_EXE) $(ext) pod/buildtoc
	$(RUN_PERL) -f -Ilib pod/buildtoc --build-toc -q

pod/perlapi.pod: pod/perlintern.pod

pod/perlintern.pod: $(MINIPERL_EXE) autodoc.pl embed.fnc
	$(MINIPERL) autodoc.pl

pod/perlmodlib.pod: $(MINIPERL_EXE) pod/perlmodlib.PL MANIFEST
	$(MINIPERL) $(Icwd) pod/perlmodlib.PL -q

pod/perldelta.pod: pod/perl5122delta.pod
	$(LNS) perl5122delta.pod pod/perldelta.pod

extra.pods: $(MINIPERL_EXE)
	-@test ! -f extra.pods || rm -f `cat extra.pods`
	-@rm -f extra.pods
	-@for x in `grep -l '^=[a-z]' README.* | grep -v README.vms` ; do \
	    nx=`echo $$x | sed -e "s/README\.//"`; \
	    $(LNS) ../$$x "pod/perl"$$nx".pod" ; \
	    echo "pod/perl"$$nx".pod" >> extra.pods ; \
	done

extras.make: $(PERL_EXE)
	-@test ! -s extras.lst || PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(RUN_PERL) -Ilib -MCPAN -e '@ARGV&&make(@ARGV)' `cat extras.lst`

extras.test: $(PERL_EXE)
	-@test ! -s extras.lst || PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(RUN_PERL) -Ilib -MCPAN -e '@ARGV&&test(@ARGV)' `cat extras.lst`

extras.install: $(PERL_EXE)
	-@test ! -s extras.lst || PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(RUN_PERL) -Ilib -MCPAN -e '@ARGV&&install(@ARGV)' `cat extras.lst`

.PHONY: install install-strip install-all install-verbose install-silent \
	no-install install.perl install.man install.html

META.yml:	Porting/makemeta Porting/Maintainers.pl Porting/Maintainers.pm
	PATH="`pwd`:${PATH}" PERL5LIB="`pwd`/lib" $(RUN_PERL) -Ilib Porting/makemeta

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
	$(RUN_PERL) installperl --destdir=$(DESTDIR) $(INSTALLFLAGS) $(STRIPFLAGS)
	-@test ! -s extras.lst || $(MAKE) extras.install

install.man:	all installman
	$(RUN_PERL) installman --destdir=$(DESTDIR) $(INSTALLFLAGS)

# XXX Experimental. Hardwired values, but useful for testing.
# Eventually Configure could ask for some of these values.
install.html: all installhtml
	-@test -f README.vms && cd vms && $(LNS) ../README.vms README_vms.pod && cd ..
	$(RUN_PERL) installhtml   \
      --podroot=. --podpath=. --recurse  \
      --htmldir=$(privlib)/html   \
      --htmlroot=$(privlib)/html  \
      --splithead=pod/perlipc     \
      --splititem=pod/perlfunc    \
      --libpods=perlfunc:perlguts:perlvar:perlrun:perlop \
      --ignore=Porting/Maintainers.pm,Porting/pumpkin.pod,Porting/repository.pod \
      --verbose


# I now supply perly.c with the kits, so the following section is
# used only if you force bison to run by saying
# 	make regen_perly
# You normally shouldn't remake perly.[ch].

.PHONY: regen_perly

run_byacc:
	@echo "run_byacc is obsolete; try 'make regen_perly' instead"

# this outputs perly.h, perly.act and perly.tab
regen_perly:
	perl regen_perly.pl

# We don't want to regenerate perly.c and perly.h, but they might
# appear out-of-date after a patch is applied or a new distribution is
# made.
perly.c: perly.y
	-@sh -c true

perly.h: perly.y
	-@sh -c true

# No compat3.sym here since and including the 5.004_50.
# No interp.sym since 5.005_03.
SYM  = global.sym globvar.sym perlio.sym pp.sym

SYMH = perlvars.h intrpvar.h

CHMOD_W = chmod +w

# The following files are generated automatically
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
		perlapi.h perlapi.c regnodes.h \
		warnings.h lib/warnings.pm

.PHONY: regen_headers regen_all

regen:	FORCE
	-perl regen.pl

regen_headers:	FORCE
	-perl regen.pl -v

regen_all: regen

.PHONY:	manisort manicheck

manisort:	FORCE
	@perl Porting/manisort -q || (echo "WARNING: re-sorting MANIFEST"; \
		perl Porting/manisort -q -o MANIFEST; sh -c true)

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

$(DYNALOADER):	$(MINIPERL_EXE) preplibrary FORCE $(nonxs_ext)
	$(MINIPERL) make_ext.pl $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL) LINKTYPE=static $(STATIC_LDFLAGS)

d_dummy $(dynamic_ext):	$(MINIPERL_EXE) preplibrary makeppport $(DYNALOADER) FORCE $(PERLEXPORT)
	$(MINIPERL) make_ext.pl $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL) LINKTYPE=dynamic

s_dummy $(static_ext):	$(MINIPERL_EXE) preplibrary makeppport $(DYNALOADER) FORCE
	$(MINIPERL) make_ext.pl $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL) LINKTYPE=static $(STATIC_LDFLAGS)

n_dummy $(nonxs_ext):	$(MINIPERL_EXE) preplibrary FORCE
	$(MINIPERL) make_ext.pl $@ MAKE=$(MAKE) LIBPERL_A=$(LIBPERL)
!NO!SUBS!

$spitshell >>$Makefile <<EOF
$extra_dep
EOF

$spitshell >>$Makefile <<'!NO!SUBS!'

.PHONY: printconfig
printconfig:
	@eval `$(RUN_PERL) -Ilib -V:$(CONFIGVAR)`; echo $$$(CONFIGVAR)

.PHONY: clean _tidy _mopup _cleaner1 _cleaner2 \
	realclean _realcleaner clobber _clobber \
	distclean veryclean _verycleaner

clean:		_tidy _mopup 

realclean:	_realcleaner _mopup
	@echo "Note that '$(MAKE) realclean' does not delete config.sh or Policy.sh"

_clobber:
	-@rm -f Cross/run-* Cross/to-* Cross/from-*
	rm -f t/test_state
	rm -f config.sh cppstdin Policy.sh extras.lst

clobber:	_realcleaner _mopup _clobber

distclean:	clobber

# Like distclean but also removes emacs backups and *.orig.
veryclean:	_verycleaner _mopup _clobber
	-@rm -f Obsolete Wanted

# Do not 'make _mopup' directly.
_mopup:
	rm -f *$(OBJ_EXT) *$(LIB_EXT) all perlmain.c opmini.c perlmini.c uudmap.h generate_uudmap$(EXE_EXT) bitcount.h
	-rmdir .depending
	-@test -f extra.pods && rm -f `cat extra.pods`
	-@test -f vms/README_vms.pod && rm -f vms/README_vms.pod
	-rm -f perl.exp ext.libs $(generated_pods) uni.data opmini.o perlmini.o
	-rm -f perl.export perl.dll perl.libexp perl.map perl.def
	-rm -f perl.loadmap miniperl.loadmap perl.prelmap miniperl.prelmap
	-rm -f perl.third lib*.so.perl.third perl.3log t/perl.third t/perl.3log
	-rm -f perl.pixie lib*.so.perl.pixie lib*.so.Addrs
	-rm -f perl.Addrs perl.Counts t/perl.Addrs t/perl.Counts *perl.xok
	-rm -f cygwin.c libperl*.def libperl*.dll cygperl*.dll *.exe.stackdump
	-rm -f $(PERL_EXE) $(MINIPERL_EXE) $(LIBPERL) libperl.* microperl
	-rm -f opcode.h-old opnames.h-old pp.sym-old pp_proto.h-old
	-rm -f $(DTRACE_H) runtests

# Do not 'make _tidy' directly.
_tidy:
	-cd pod; $(LDLIBPTH) $(MAKE) clean
	-cd utils; $(LDLIBPTH) $(MAKE) clean
	-cd x2p; $(LDLIBPTH) $(MAKE) clean
	-@for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) $(nonxs_ext) ; do \
	$(MINIPERL) make_ext.pl --target=clean $$x MAKE=$(MAKE) ; \
	done

_cleaner1:
	-cd os2; rm -f Makefile
	-cd pod; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-cd utils; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-cd x2p; $(LDLIBPTH) $(MAKE) $(CLEAN)
	-@if test -f $(MINIPERL_EXE) ; then \
	for x in $(DYNALOADER) $(dynamic_ext) $(static_ext) $(nonxs_ext) ; do \
	$(MINIPERL) make_ext.pl --target=$(CLEAN) $$x MAKE=$(MAKE) ; \
	done ; \
	else \
	sh $(CLEAN).sh ; \
	fi
	rm -f realclean.sh veryclean.sh
	-for file in `find cpan dist ext -name ppport.h` ; do rm -f $$file; done

# Dear POSIX, thanks for making the default to xargs to be
# run once if nothhing is passed in. It is such a great help.

# Some systems do not support "?", so keep these files separate.
_cleaner2:
	-rm -f core.*perl.*.? t/core.perl.*.? .?*.c
	rm -f core *perl.core t/core t/*perl.core core.* t/core.*
	rm -f t/misctmp* t/forktmp* t/tmp* t/c t/$(PERL_EXE) t/rantests
	rm -f so_locations $(LIBPERL_NONSHR) $(MINIPERL_NONSHR)
	rm -rf $(addedbyconf)
	rm -f $(FIRSTMAKEFILE) $(FIRSTMAKEFILE).old makefile.old
	rm -f $(private)
	rm -rf $(unidatafiles) $(unidatadirs)
	rm -rf lib/auto
	rm -f lib/.exists lib/*/.exists lib/*/*/.exists
	rm -f h2ph.man pstruct
	rm -rf .config
	rm -f preload lib/re.pm
	rm -rf lib/Encode lib/Compress lib/Hash lib/re
	rm -rf lib/TAP lib/Module/Pluggable lib/App
	rm -rf lib/mro
	rm -rf lib/IO/Compress lib/IO/Uncompress
	rm -f lib/ExtUtils/ParseXS/t/XSTest.c
	rm -f lib/ExtUtils/ParseXS/t/XSTest$(OBJ_EXT)
	rm -f lib/ExtUtils/ParseXS/t/XSTest$(DLSUFFIX)
	rm -fr lib/B
	rm -fr lib/CPAN lib/CPANPLUS
	rm -fr lib/ExtUtils/CBuilder
	-rmdir cpan/CPANPLUS-Dist-Build/t/dummy-cpanplus cpan/CPANPLUS/t/dummy-cpanplus cpan/CPANPLUS/t/dummy-localmirror
	-rmdir ext/B/lib
	-rmdir lib/Archive/Tar lib/Archive lib/Attribute
	-rmdir lib/CGI
	-rmdir lib/Data lib/Devel lib/Digest
	-rmdir lib/ExtUtils/Command lib/ExtUtils/Constant lib/ExtUtils/Liblist lib/ExtUtils/MakeMaker
	-rmdir lib/File/Spec lib/Filter/Util lib/Filter
	-rmdir lib/I18N/LangTags lib/IO/Socket lib/IO lib/IPC
	-rmdir lib/List/Util lib/List
	-rmdir lib/Locale/Maketext lib/Locale
	-rmdir lib/Log/Message lib/Log
	-rmdir lib/Math/Big* lib/Math
	-rmdir lib/Memoize lib/MIME
	-rmdir lib/Module/Build/Platform lib/Module/Build lib/Module/Load lib/Module
	-rmdir lib/Net/FTP lib/Object
	-rmdir lib/Parse/CPAN lib/Parse
	-rmdir lib/PerlIO/via lib/PerlIO
	-rmdir lib/Package lib/Params
	-rmdir lib/Pod/Perldoc lib/Pod/Simple lib/Pod/Text
	-rmdir lib/Sys lib/Scalar/Util lib/Scalar
	-rmdir lib/Term/UI lib/Thread
	-rmdir lib/Test/Builder/Tester lib/Test/Builder lib/Test
	-rmdir lib/Unicode/Collate
	-rmdir lib/XS/APItest lib/XS
	-rmdir lib/inc/latest lib/inc
	-rmdir lib/autodie/exception lib/autodie lib/encoding lib/threads
	-rm -f lib/ExtUtils/CBuilder/t/libcompilet.dll.a
	-rm -f lib/ExtUtils/ParseXS/t/libXSTest.dll.a

_realcleaner:
	@$(LDLIBPTH) $(MAKE) _cleaner1 CLEAN=distclean
	@$(LDLIBPTH) $(MAKE) _cleaner2

_verycleaner:
	@$(LDLIBPTH) $(MAKE) _cleaner1 CLEAN=veryclean
	@$(LDLIBPTH) $(MAKE) _cleaner2
	-rm -f *~ *.orig */*~ */*.orig */*/*~ */*/*.orig

.PHONY: lint
lint: $(c)
	rm -f *.ln
	lint $(lintflags) -DPERL_CORE -D_REENTRANT -DDEBUGGING -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 $(c)

.PHONY: splint
splint: $(c)
	splint $(splintflags) -DPERL_CORE -D_REENTRANT -DDEBUGGING -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 $(splintfiles)

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

runtests: runtests.SH config.sh
	sh ./runtests.SH

.PHONY: test check test_prep test_prep_nodll test_prep_pre \
	test_prep_reonly test_tty test-tty test_notty test-notty \
	utest ucheck test.utf8 check.utf8 test.torture torturetest \
	test.utf16 check.utf16 utest.utf16 ucheck.utf16 \
	test.third check.third utest.third ucheck.third test_notty.third \
	test.deparse test_notty.deparse test_harness test_harness_notty \
	minitest coretest test.taintwarn test-reonly _test

_test:
	echo >&2 The _test target is deprecated. Please upgrade your smoker
	PERL=./perl $(RUN_TESTS) choose

# Cannot delegate rebuilding of t/perl to make
# to allow interlaced test and minitest

# Architecture-neutral stuff:

test_prep_pre: preplibrary utilities $(nonxs_ext)

test_prep: test_prep_pre $(MINIPERL_EXE) $(unidatafiles) $(PERL_EXE) $(dynamic_ext) $(TEST_PERL_DLL) runtests
	cd t && (rm -f $(PERL_EXE); $(LNS) ../$(PERL_EXE) $(PERL_EXE))

test_prep_reonly: $(MINIPERL_EXE) $(PERL_EXE) $(dynamic_ext_re) $(TEST_PERL_DLL)
	$(MINIPERL) make_ext.pl $(dynamic_ext_re) MAKE=$(MAKE) LIBPERL_A=$(LIBPERL) LINKTYPE=dynamic
	cd t && (rm -f $(PERL_EXE); $(LNS) ../$(PERL_EXE) $(PERL_EXE))

test check: test_prep
	$(RUN_TESTS) choose

test_tty: test_prep
	$(RUN_TESTS) tty

test_notty: test_prep
	$(RUN_TESTS) no-tty

utest ucheck test.utf8 check.utf8: test_prep
	TEST_ARGS=-utf8 $(RUN_TESTS) choose

coretest: test_prep
	TEST_ARGS=-core $(RUN_TESTS) choose

test-prep:	test_prep

test-tty:	test_tty

test-notty:	test_notty

# Torture testing

test.torture torturetest:	test_prep
	TEST_ARGS=-torture $(RUN_TESTS) choose

# Targets for UTF16 testing:

minitest.utf16: minitest.prep
	- cd t && (rm -f $(PERL_EXE); $(LNS) ../$(MINIPERL_EXE) $(PERL_EXE)) \
		&& $(RUN_PERL) TEST -minitest -utf16 base/*.t comp/*.t cmd/*.t run/*.t io/*.t op/*.t uni/*.t </dev/tty

test.utf16 check.utf16: test_prep
	TEST_ARGS=-utf16 $(RUN_TESTS) choose

utest.utf16 ucheck.utf16: test_prep
	TEST_ARGS="-utf8 -utf16" $(RUN_TESTS) choose

# Targets for valgrind testing:

test_prep.valgrind: test_prep perl.valgrind

test.valgrind check.valgrind:	test_prep perl.valgrind.config
	PERL_VALGRIND=1 VALGRIND='$(VALGRIND)' $(RUN_TESTS) choose

utest.valgrind ucheck.valgrind: test_prep.valgrind perl.valgrind.config
	PERL_VALGRIND=1 TEST_ARGS=-utf8 $(RUN_TESTS) choose

test_notty.valgrind: test_prep.valgrind perl.valgrind.config
	PERL_VALGRIND=1 $(RUN_TESTS) no-tty

# Targets for Third Degree testing.

test_prep.third: test_prep perl.third
	cd t && (rm -f ./perl.third$(EXE_EXT); $(LNS) ../perl.third$(EXE_EXT) perl.third(EXE_EXT))

test.third check.third:	test_prep.third perl.third
	PERL=./perl.third PERL_3LOG=1 $(RUN_TESTS) choose

utest.third ucheck.third: test_prep.third perl.third
	PERL=./perl.third PERL_3LOG=1 TEST_ARGS=-utf8 $(RUN_TESTS) choose

test_notty.third: test_prep.third perl.third
	PERL=./perl.third PERL_3LOG=1 $(RUN_TESTS) choose

# Targets for Deparse testing.

test.deparse:	test_prep
	TEST_ARGS=-deparse $(RUN_TESTS) choose

test_notty.deparse:	test_prep
	TEST_ARGS=-deparse $(RUN_TESTS) no-tty

# Targets to run the test suite with -t

test.taintwarn:	test_prep
	TEST_ARGS=-taintwarn $(RUN_TESTS) choose

minitest.prep:
	-@test -f lib/Config.pm || $(MAKE) lib/Config.pm $(unidatafiles)
	@echo " "
	@echo "You may see some irrelevant test failures if you have been unable"
	@echo "to build lib/Config.pm, or the Unicode data files."
	@echo " "

# Can't depend on lib/Config.pm because that might be where miniperl
# is crashing.
minitest: $(MINIPERL_EXE) lib/re.pm minitest.prep
	- cd t && (rm -f $(PERL_EXE); $(LNS) ../$(MINIPERL_EXE) $(PERL_EXE)) \
		&& $(RUN_PERL) TEST -minitest base/*.t comp/*.t cmd/*.t run/*.t io/*.t re/*.t op/*.t uni/*.t </dev/tty

# Test via harness

test_harness: test_prep
	TESTFILE=harness $(RUN_TESTS) choose

test_harness_notty: test_prep
	HARNESS_NOTTY=1 TESTFILE=harness $(RUN_TESTS) choose

test-reonly: test_prep_reonly
	TEST_ARGS='-re \bre\/' TESTFILE=harness $(RUN_TESTS) choose


# Porting tests (well-formedness of pod, manifest, etc)

test_porting: test_prep
	cd t && $(RUN_PERL) harness porting/*.t

# Handy way to run perlbug -ok without having to install and run the
# installed perlbug. We don't re-run the tests here - we trust the user.
# Please *don't* use this unless all tests pass.
# If you want to report test failures, use "make nok" instead.

.PHONY: ok okfile oknack okfilenack nok nokfile noknack nokfilenack

ok:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -ok -s '(UNINSTALLED)'

okfile:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -F perl.ok

oknack:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -A

okfilenack:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -ok -s '(UNINSTALLED)' -F perl.ok -A

nok:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -nok -s '(UNINSTALLED)'

nokfile:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -F perl.nok

noknack:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -A

nokfilenack:	utilities
	$(RUN_PERL) -Ilib utils/perlbug -nok -s '(UNINSTALLED)' -F perl.nok -A

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

# Let's hope make will not go into an infinite loop on case-unsensitive systems
# This may also fail if . is in the head of the path, since perl will
# require -Ilib
tags:	TAGS
	perl emacs/e2ctags.pl TAGS > tags

ctags:
	ctags -f Tags -N --totals --languages=c --langmap=c:+.h --exclude=opmini.c --exclude=perlmini.c *.c *.h

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

# ex: set ts=8 sts=4 sw=4 noet:

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
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac
echo "Extracting cflags (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.
rm -f cflags
$spitshell >cflags <<!GROK!THIS!
$startsh
!GROK!THIS!

: In the following dollars and backticks do not need the extra backslash.
$spitshell >>cflags <<'!NO!SUBS!'
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

: syntax: cflags [optimize=XXX] [file[.suffix]]
: displays the compiler command line for file

case "X$1" in
Xoptimize=*|X"optimize=*")
	eval "$1"
	shift
	;;
esac

also=': '
case $# in
1) also='echo 1>&2 "	  CCCMD = "'
esac

case $# in
0) set *.c; echo "The current C flags are:" ;;
esac

set `echo "$* " | sed -e 's/\.[oc] / /g' -e 's/\.obj / /g' -e "s/\\$obj_ext / /g"`

for file do

    case "$#" in
    1) ;;
    *) echo $n "    $file.c	$c" ;;
    esac

    : allow variables like toke_cflags to be evaluated

    if echo $file | grep -v / >/dev/null
    then
      eval 'eval ${'"${file}_cflags"'-""}'
    fi

    : or customize here

    case "$file" in
    DB_File) ;;
    GDBM_File) ;;
    NDBM_File) ;;
    ODBM_File) ;;
    POSIX) ;;
    SDBM_File) ;;
    av) ;;
    byterun) ;;
    deb) ;;
    dl) ;;
    doio) ;;
    doop) ;;
    dump) ;;
    gv) ;;
    hv) ;;
    locale) ;;
    main) ;;
    malloc) ;;
    mg) ;;
    miniperlmain) ;;
    numeric) ;;
    op) ;;
    perl) ;;
    perlapi) ;;
    perlmain) ;;
    perly) ;;
    pp) ;;
    pp_ctl) ;;
    pp_hot) ;;
    pp_pack) ;;
    pp_sys) ;;
    regcomp) ;;
    regexec) ;;
    run) ;;
    scope) ;;
    sv) ;;
    taint) ;;
    toke) ;;
    usersub) ;;
    util) ;;
    *) ;;
    esac

# Add -Wall for the core and core modules iff gcc and not already -Wall
warn=''
case "$gccversion" in
'') ;;
*)  case "$ccflags" in
    *-Wall*) ;;
# Can't add -ansi here because it will fail e.g. in Solaris.
# Off_t/off_t is a struct in Solaris with largefiles, and with -ansi
# that struct cannot be compared with a flat integer, such as a STRLEN.
# The -ansi will also cause a lot of noise in Solaris because of:
# /usr/include/sys/resource.h:148: warning: `struct rlimit64' declared inside parameter list
    *) warn='-Wall' ;;
    esac
    ;;
esac

    : Can we perhaps use $ansi2knr here
    echo "$cc -c -DPERL_CORE $ccflags $optimize $warn"
    eval "$also "'"$cc -DPERL_CORE -c $ccflags $optimize $warn"'

    . $TOP/config.sh

done
!NO!SUBS!
chmod 755 cflags
$eunicefix cflags

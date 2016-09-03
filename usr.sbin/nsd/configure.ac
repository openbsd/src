dnl
dnl Some global settings
dnl

sinclude(acx_nlnetlabs.m4)

AC_INIT(NSD,4.1.12,nsd-bugs@nlnetlabs.nl)
AC_CONFIG_HEADER([config.h])

CFLAGS="$CFLAGS"
AC_AIX
if test "$ac_cv_header_minix_config_h" = "yes"; then
	AC_DEFINE(_NETBSD_SOURCE,1, [Enable for compile on Minix])
fi

dnl
dnl By default set $sysconfdir to /etc and $localstatedir to /var
dnl
case "$prefix" in
	NONE)
		case "$sysconfdir" in
			'${prefix}/etc')
				sysconfdir=/etc
			;;
                esac
		case "$localstatedir" in
			'${prefix}/var')
				localstatedir=/var
			;;
		esac
	;;
esac

#
# Determine configuration directory
#
configdir=$sysconfdir/nsd
AC_ARG_WITH([configdir],
	AC_HELP_STRING([--with-configdir=dir], [NSD configuration directory]),
	[configdir=$withval])
AC_DEFINE_UNQUOTED(CONFIGDIR, ["`eval echo $configdir`"], [NSD config dir])
AC_SUBST(configdir)

#
# Determine configuration file
nsd_conf_file=${configdir}/nsd.conf
AC_ARG_WITH([nsd_conf_file],
	AC_HELP_STRING([--with-nsd_conf_file=path], [Pathname to the NSD configuration file]),
	[nsd_conf_file=$withval])
AC_SUBST(nsd_conf_file)
# the eval is to evaluate shell expansion twice, once
# for $nsd_conf_file and once for the ${prefix} within it.
AC_DEFINE_UNQUOTED(CONFIGFILE, ["`eval echo $nsd_conf_file`"], [Pathname to the NSD configuration file])

#
# Default logfile
#
logfile=${localstatedir}/log/nsd.log
AC_ARG_WITH([logfile],
	AC_HELP_STRING([--with-logfile=path], [Pathname to the default log file]),
	[logfile=$withval])
AC_SUBST(logfile)

#
# Database directory
#
dbdir=${localstatedir}/db/nsd

#
# Determine the pidfile location. Check if /var/run exists, if so set pidfile
# to /var/run/nsd.pid by default
#
if test -d ${localstatedir}/run; then
	pidfile=${localstatedir}/run/nsd.pid
else
	pidfile=${dbdir}/nsd.pid
fi
AC_ARG_WITH([pidfile],
	AC_HELP_STRING([--with-pidfile=path], [Pathname to the NSD pidfile]),
	[pidfile=$withval])
AC_SUBST(pidfile)
AC_DEFINE_UNQUOTED(PIDFILE, ["`eval echo $pidfile`"], [Pathname to the NSD pidfile])

#
# Determine location of nsd.db
#
dbfile=${dbdir}/nsd.db
AC_ARG_WITH([dbfile],
	AC_HELP_STRING([--with-dbfile=path], [Pathname to the NSD database]),
	[dbfile=$withval])
AC_SUBST(dbfile)
AC_DEFINE_UNQUOTED(DBFILE, ["`eval echo $dbfile`"], [Pathname to the NSD database])

if test -n "$dbfile"; then
	dbdir=`dirname $dbfile`
fi
AC_SUBST(dbdir)

piddir=`dirname $pidfile`
AC_SUBST(piddir)

#
# Determine the default directory for the zone files
#
zonesdir=$configdir
AC_ARG_WITH([zonesdir],
	AC_HELP_STRING([--with-zonesdir=dir], [NSD default location for zone files]),
	[zonesdir=$withval])
AC_SUBST(zonesdir)
AC_DEFINE_UNQUOTED(ZONESDIR, ["`eval echo $zonesdir`"], [NSD default location for zone files. Empty string or NULL to disable.])

# default xfrd file location.
xfrdfile=${dbdir}/xfrd.state
AC_ARG_WITH([xfrdfile], AC_HELP_STRING([--with-xfrdfile=path], 
	[Pathname to the NSD xfrd zone timer state file]), [xfrdfile=$withval])
AC_DEFINE_UNQUOTED(XFRDFILE, ["`eval echo $xfrdfile`"], [Pathname to the NSD xfrd zone timer state file.])
AC_SUBST(xfrdfile)

# default zonelist file location.
zonelistfile=${dbdir}/zone.list
AC_ARG_WITH([zonelistfile], AC_HELP_STRING([--with-zonelistfile=path], 
	[Pathname to the NSD zone list file]), [zonelistfile=$withval])
AC_DEFINE_UNQUOTED(ZONELISTFILE, ["`eval echo $zonelistfile`"], [Pathname to the NSD zone list file.])
AC_SUBST(zonelistfile)

# default xfr dir location.
xfrdir="/tmp"
AC_ARG_WITH([xfrdir], AC_HELP_STRING([--with-xfrdir=path], 
	[Pathname to where the NSD transfer dir is created]), [xfrdir=$withval])
AC_DEFINE_UNQUOTED(XFRDIR, ["`eval echo $xfrdir`"], [Pathname to where the NSD transfer dir is created.])
AC_SUBST(xfrdir)

# nsd sbin location. tmpinstantiate execprefix with defaults if not yet done.
if test "x${exec_prefix}" = "xNONE"; then
	if test "x${prefix}" = "xNONE"; then exec_prefix="$ac_default_prefix"
	else exec_prefix="${prefix}"; fi
	nsd_start_path="`eval echo $sbindir`/nsd"
	exec_prefix="NONE"
else
	nsd_start_path="`eval echo $sbindir`/nsd"
fi
AC_DEFINE_UNQUOTED(NSD_START_PATH, ["$nsd_start_path"], [Pathname to start nsd from nsd-control])

#
# Determine default chroot directory
#
AC_ARG_WITH([chroot],
	AC_HELP_STRING([--with-chroot=dir], [NSD default chroot directory]),
	[
		chrootdir=$withval
		AC_DEFINE_UNQUOTED(CHROOTDIR, ["`eval echo $chrootdir`"], [NSD default chroot directory])
	])
AC_SUBST(chrootdir)

#
# Determine the user name to drop privileges to
#
user=nsd
AC_ARG_WITH([user],
	AC_HELP_STRING([--with-user=username], [User name or ID to answer the queries with]),
	[user=$withval])
AC_SUBST(user)
AC_DEFINE_UNQUOTED(USER, ["$user"], [the user name to drop privileges to])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL
AC_PROG_LEX
AC_PROG_YACC

if test "$LEX" != ":" -a "$LEX" != ""; then
  # Check if lex defines yy_current_buffer, because 2.4.6 and older use it,
  # but later could define it as a macro and then we should not redefine it.
  AC_MSG_CHECKING(if lex defines yy_current_buffer)
  cat <<EOF >conftest.lex
%%
EOF
  $LEX -i -t conftest.lex >> conftest.c
  if grep "^#define yy_current_buffer" conftest.c >/dev/null; then
	  AC_DEFINE_UNQUOTED(LEX_DEFINES_YY_CURRENT_BUFFER, 1, [If flex defines yy_current_buffer as a macro])
	AC_MSG_RESULT(yes)
  else
	AC_MSG_RESULT(no)
  fi
  rm -f conftest.lex conftest.c
fi

AC_DEFUN([AC_CHECK_FORMAT_ATTRIBUTE],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether the C compiler (${CC-cc}) accepts the "format" attribute)
AC_CACHE_VAL(ac_cv_c_format_attribute,
[ac_cv_c_format_attribute=no
AC_TRY_COMPILE(
[#include <stdio.h>
void f (char *format, ...) __attribute__ ((format (printf, 1, 2)));
void (*pf) (char *format, ...) __attribute__ ((format (printf, 1, 2)));
], [
   f ("%s", "str");
],
[ac_cv_c_format_attribute="yes"],
[ac_cv_c_format_attribute="no"])
])

AC_MSG_RESULT($ac_cv_c_format_attribute)
if test $ac_cv_c_format_attribute = yes; then
  AC_DEFINE(HAVE_ATTR_FORMAT, 1, [Whether the C compiler accepts the "format" attribute])
fi
])dnl

AC_DEFUN([AC_CHECK_UNUSED_ATTRIBUTE],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether the C compiler (${CC-cc}) accepts the "unused" attribute)
AC_CACHE_VAL(ac_cv_c_unused_attribute,
[ac_cv_c_unused_attribute=no
AC_TRY_COMPILE(
[#include <stdio.h>
void f (char *u __attribute__((unused)));
], [
   f ("x");
],
[ac_cv_c_unused_attribute="yes"],
[ac_cv_c_unused_attribute="no"])
])

AC_MSG_RESULT($ac_cv_c_unused_attribute)
if test $ac_cv_c_unused_attribute = yes; then
  AC_DEFINE(HAVE_ATTR_UNUSED, 1, [Whether the C compiler accepts the "unused" attribute])
fi
])dnl

AC_DEFUN([CHECK_COMPILER_FLAG],
[
AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether $CC supports -$1)
cache=`echo $1 | sed 'y%.=/+-%___p_%'`
AC_CACHE_VAL(cv_prog_cc_flag_$cache,
[
echo 'void f(){}' >conftest.c
if test -z "`$CC -$1 -c conftest.c 2>&1`"; then
eval "cv_prog_cc_flag_$cache=yes"
else
eval "cv_prog_cc_flag_$cache=no"
fi
rm -f conftest*
])
if eval "test \"`echo '$cv_prog_cc_flag_'$cache`\" = yes"; then
AC_MSG_RESULT(yes)
:
$2
else
AC_MSG_RESULT(no)
:
$3
fi
])

AC_DEFUN([AC_CHECK_CTIME_R],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether ctime_r works with two arguments)
AC_CACHE_VAL(ac_cv_c_ctime_c,
[ac_cv_c_ctime_c=no
AC_TRY_COMPILE(
[#include <time.h>
void testing (void) { time_t clock; char current_time[40]; ctime_r(&clock, current_time); }],
[
        testing();
],
[ac_cv_c_ctime_c="yes"],
[ac_cv_c_ctime_c="no"])
])

AC_MSG_RESULT($ac_cv_c_ctime_c)
if test $ac_cv_c_ctime_c = no; then
        CPPFLAGS="$CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
fi
])dnl

# Checks for typedefs, structures, and compiler characteristics.
# allow user to override the -g -O2 flags.
if test "x$CFLAGS" = "x" ; then
	ACX_CHECK_COMPILER_FLAG(g, [CFLAGS="$CFLAGS -g"])
	# we do not use O3 because it causes miscompilations.
	ACX_CHECK_COMPILER_FLAG(O2, [CFLAGS="$CFLAGS -O2"])
	ACX_CHECK_FLTO
	ACX_CHECK_PIE
	ACX_CHECK_RELRO_NOW
fi
AC_C_CONST
AC_C_INLINE
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_OFF_T

AC_CHECK_FORMAT_ATTRIBUTE
AC_CHECK_UNUSED_ATTRIBUTE
ACX_CHECK_MEMCMP_SIGNED
AC_CHECK_CTIME_R

# Checks for libraries.

# Check for SSL, original taken from
# http://www.gnu.org/software/ac-archive/htmldoc/check_ssl.html and
# modified for NSD.
AC_DEFUN([CHECK_SSL], [
    AC_ARG_WITH(ssl, AC_HELP_STRING([--with-ssl=pathname],
                [enable SSL (will check /usr/local/ssl
                /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr)]),[
        ],[
            withval="yes"
        ])
    if test x_$withval != x_no; then
        AC_MSG_CHECKING(for SSL)
        if test x_$withval = x_ -o x_$withval = x_yes; then
            withval="/usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/sfw /usr/local /usr"
        fi
        for dir in $withval; do
            ssldir="$dir"
            if test -f "$dir/include/openssl/ssl.h"; then
                found_ssl="yes";
                AC_DEFINE_UNQUOTED([HAVE_SSL], [], [Define if you have the SSL libraries installed.])
                if test x_$ssldir != x_/usr; then
                    CPPFLAGS="$CPPFLAGS -I$ssldir/include";
                fi
                break;
            fi
        done 
        if test x_$found_ssl != x_yes; then
            AC_MSG_ERROR(Cannot find the SSL libraries in $withval)
        else
            AC_MSG_RESULT(found in $ssldir)
            HAVE_SSL=yes 
            if test x_$ssldir != x_/usr; then
                LDFLAGS="$LDFLAGS -L$ssldir/lib";
            fi
	    if test x_$ssldir = x_/usr/sfw; then
		LDFLAGS="$LDFLAGS -R$ssldir/lib";
	    fi
        fi
        AC_SUBST(HAVE_SSL)
    fi
])dnl

# check for libevent
AC_ARG_WITH(libevent, AC_HELP_STRING([--with-libevent=pathname],
    [use libevent (will check /usr/local /opt/local /usr/lib /usr/pkg /usr/sfw /usr  or you can specify an explicit path), useful when the zone count is high.]),
    [ ],[ withval="yes" ])
if test x_$withval = x_yes -o x_$withval != x_no; then
        AC_MSG_CHECKING(for libevent)
        if test x_$withval = x_ -o x_$withval = x_yes; then
            withval="/usr/local /opt/local /usr/lib /usr/pkg /usr/sfw /usr"
        fi
        for dir in $withval; do
            thedir="$dir"
            if test -f "$dir/include/event.h" -o -f "$dir/include/event2/event.h"; then
                found_libevent="yes"
		dnl assume /usr is in default path.
		if test "$thedir" != "/usr"; then
                    CPPFLAGS="$CPPFLAGS -I$thedir/include"
		fi
                break;
            fi
        done
        if test x_$found_libevent != x_yes; then
		if test -f "$dir/event.h" -a \( -f "$dir/libevent.la" -o -f "$dir/libev.la" \) ; then
			# libevent source directory
            		AC_MSG_RESULT(found in $thedir)
                	CPPFLAGS="$CPPFLAGS -I$thedir -I$thedir/include"
			# remove evdns from linking
			ev_files_o=`ls $thedir/*.o | grep -v evdns\.o | grep -v bufferevent_openssl\.o`
			cp $ev_files_o .
			LDFLAGS="$ev_files_o $LDFLAGS -lm"
		else
            		AC_MSG_ERROR([Cannot find the libevent library.
You can restart ./configure --with-libevent=no to use a builtin alternative.])
		fi
        else
            AC_MSG_RESULT(found in $thedir)
	    dnl if event2 exists and no event lib in dir itself, use subdir
	    if test ! -f $thedir/lib/libevent.a -a ! -f $thedir/lib/libevent.so -a -d "$thedir/lib/event2"; then
		LDFLAGS="$LDFLAGS -L$thedir/lib/event2"
		ACX_RUNTIME_PATH_ADD([$thedir/lib/event2])
	    else
		    dnl assume /usr is in default path, do not add "".
		    if test "$thedir" != "/usr" -a "$thedir" != ""; then
			LDFLAGS="$LDFLAGS -L$thedir/lib"
			ACX_RUNTIME_PATH_ADD([$thedir/lib])
		    fi
	    fi
        fi
	# check for library used by libevent after 1.3c
	AC_SEARCH_LIBS([clock_gettime], [rt])

	# is the event.h header libev or libevent?
	AC_CHECK_HEADERS([event.h],,, [AC_INCLUDES_DEFAULT])
	AC_CHECK_DECL(EV_VERSION_MAJOR, [
		AC_SEARCH_LIBS(event_set, [ev])
	],[
		AC_SEARCH_LIBS(event_set, [event])
	],[AC_INCLUDES_DEFAULT
#include <event.h>
	])
	AC_CHECK_FUNCS([event_base_free]) # only in libevent 1.2 and later
	AC_CHECK_FUNCS([event_base_once]) # only in libevent 1.4.1 and later
	AC_CHECK_FUNCS([event_base_new]) # only in libevent 1.4.1 and later
	AC_CHECK_FUNCS([event_base_get_method]) # only in libevent 1.4.3 and later
	AC_CHECK_FUNCS([ev_loop]) # only in libev. (tested on 3.51)
	AC_CHECK_FUNCS([ev_default_loop]) # only in libev. (tested on 4.00)
else
	AC_DEFINE(USE_MINI_EVENT, 1, [Define if you want to use internal select based events])
fi

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([time.h arpa/inet.h signal.h string.h strings.h fcntl.h limits.h netinet/in.h netinet/tcp.h stddef.h sys/param.h sys/socket.h syslog.h unistd.h sys/select.h stdarg.h stdint.h netdb.h sys/bitypes.h tcpd.h glob.h grp.h endian.h])

AC_DEFUN([CHECK_VALIST_DEF],
[
AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(for double definition of struct va_list)
AC_CACHE_VAL(ac_cv_c_va_list_def,
[
cat >conftest.c <<EOF
#include <stdio.h>
#include <stdarg.h>
int foo(void);
EOF
if test -z "`$CC -Werror -D_XOPEN_SOURCE=600 -c conftest.c 2>&1`"; then
eval "ac_cv_c_va_list_def=no"
else
eval "ac_cv_c_va_list_def=yes"
fi
rm -f conftest*
])
if test $ac_cv_c_va_list_def = yes; then
AC_MSG_RESULT(yes)
:
AC_DEFINE_UNQUOTED([HAVE_VA_LIST_DOUBLE_DEF], [], [Define this if you have double va_list definitions.])
else
AC_MSG_RESULT(no)
:

fi
])

CHECK_VALIST_DEF

AC_DEFUN([AC_CHECK_STRPTIME],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether strptime needs defines)
AC_CACHE_VAL(ac_cv_c_strptime_needs_defs,
[
cat >conftest.c <<EOF
#include <time.h>
int testing (void) { struct tm t; const char *timestr="201201"; return strptime(timestr, "%Y%m", &t) != 0; }
EOF
if test -z "`$CC -Wall -Werror -c conftest.c 2>&1`"; then
eval "ac_cv_c_strptime_needs_defs=no"
else
eval "ac_cv_c_strptime_needs_defs=yes"
fi
rm -f conftest*
])

AC_MSG_RESULT($ac_cv_c_strptime_needs_defs)
if test $ac_cv_c_strptime_needs_defs = yes; then
AC_DEFINE_UNQUOTED([STRPTIME_NEEDS_DEFINES], 1, [strptime is available from time.h with some defines.])
fi
])dnl

AC_CHECK_STRPTIME

# check wether strptime also works
AC_DEFUN([AC_CHECK_STRPTIME_WORKS],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(whether strptime works)
if test c${cross_compiling} = cno; then
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#define _XOPEN_SOURCE 600
#include <time.h>
int main(void) { struct tm tm; char *res; 
res = strptime("20070207111842", "%Y%m%d%H%M%S", &tm);
if (!res) return 1; return 0; }
]])] , [eval "ac_cv_c_strptime_works=yes"], [eval "ac_cv_c_strptime_works=no"])
else
eval "ac_cv_c_strptime_works=maybe"
fi
AC_MSG_RESULT($ac_cv_c_strptime_works)
if test $ac_cv_c_strptime_works = no; then
AC_LIBOBJ(strptime)
else
AC_DEFINE_UNQUOTED([STRPTIME_WORKS], 1, [use default strptime.])
fi
])dnl

AC_SEARCH_LIBS(inet_pton, [nsl])
AC_SEARCH_LIBS(socket, [socket])

AC_CHECK_STRPTIME_WORKS
ACX_CHECK_NONBLOCKING_BROKEN
ACX_MKDIR_ONE_ARG

# set -I. and -Isrcdir
if test -n "$CPPFLAGS"; then
	CPPFLAGS="$CPPFLAGS -I."
else
	CPPFLAGS="-I."
fi
if test "$srcdir" != "."; then
	CPPFLAGS="$CPPFLAGS -I$srcdir"
	if test -f $srcdir/config.h; then
		AC_ERROR([$srcdir/config.h is in the way, please remove it])
	fi
fi

dnl LIBGTOP_CHECK_TYPE
dnl Stolen from Gnome's anjuta
dnl Improved version of AC_CHECK_TYPE which takes into account
dnl that we need to #include some other header files on some
dnl systems to get some types.

dnl AC_LIBGTOP_CHECK_TYPE(TYPE, DEFAULT)
AC_DEFUN([AC_LIBGTOP_CHECK_TYPE],
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

/* For Tru64 */
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2, Define "$1" to "$2" if "$1" is missing)
fi
])

AC_LIBGTOP_CHECK_TYPE(int8_t, char) 
AC_LIBGTOP_CHECK_TYPE(int16_t, short)
AC_LIBGTOP_CHECK_TYPE(int32_t, int)
AC_LIBGTOP_CHECK_TYPE(int64_t, long long)
AC_LIBGTOP_CHECK_TYPE(uint8_t, unsigned char)
AC_LIBGTOP_CHECK_TYPE(uint16_t, unsigned short)
AC_LIBGTOP_CHECK_TYPE(uint32_t, unsigned int)
AC_LIBGTOP_CHECK_TYPE(uint64_t, unsigned long long)
AC_LIBGTOP_CHECK_TYPE(socklen_t, int)
AC_LIBGTOP_CHECK_TYPE(sig_atomic_t, int)
AC_LIBGTOP_CHECK_TYPE(ssize_t, int)
AC_LIBGTOP_CHECK_TYPE(suseconds_t, time_t)

AC_CHECK_TYPE(in_addr_t, [], [AC_DEFINE([in_addr_t], [uint32_t], [in_addr_t])], [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])
ACX_CHECK_SS_FAMILY
AC_CHECK_MEMBERS([struct stat.st_mtimensec, struct stat.st_mtim.tv_nsec])

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_FUNC_FSEEKO
AC_SYS_LARGEFILE
AC_CHECK_SIZEOF(void*)
AC_CHECK_SIZEOF(off_t)
AC_CHECK_FUNCS([arc4random arc4random_uniform])
AC_CHECK_FUNCS([tzset alarm chroot dup2 endpwent gethostname memset memcpy pwrite socket strcasecmp strchr strdup strerror strncasecmp strtol writev getaddrinfo getnameinfo freeaddrinfo gai_strerror sigaction sigprocmask strptime strftime localtime_r setusercontext glob initgroups setresuid setreuid setresgid setregid getpwnam mmap ppoll clock_gettime])

AC_ARG_ENABLE(recvmmsg, AC_HELP_STRING([--enable-recvmmsg], [Enable recvmmsg and sendmmsg compilation, faster but some kernel versions may have implementation problems for IPv6]))
case "$enable_recvmmsg" in
        yes)
		AC_CHECK_FUNC([recvmmsg], [
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/socket.h>
#include <errno.h>
int main(void)
{ 
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int r = recvmmsg(s, 0, 0, 0, 0) == -1 && errno == ENOSYS;
	close(s);
	return r;
}
]])], [
AC_DEFINE([HAVE_RECVMMSG], [1], [Define if recvmmsg is implemented])], [
], [
AC_DEFINE([HAVE_RECVMMSG], [1], [Define if recvmmsg exists])]
)])
		AC_CHECK_FUNC([sendmmsg], [
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sys/socket.h>
#include <errno.h>
int main(void)
{ 
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int r = sendmmsg(s, 0, 0, 0) == -1 && errno == ENOSYS;
	close(s);
	return r;
}
]])], [
AC_DEFINE([HAVE_SENDMMSG], [1], [Define if sendmmsg is implemented])], [
], [
AC_DEFINE([HAVE_SENDMMSG], [1], [Define if sendmmsg exists])]
)])

		;;
        no|*)
                ;;
esac

# check if setreuid en setregid fail, on MacOSX10.4(darwin8).
if echo $build_os | grep darwin8 > /dev/null; then
	AC_DEFINE(DARWIN_BROKEN_SETREUID, 1, [Define this if on macOSX10.4-darwin8 and setreuid and setregid do not work])
fi

#
# Checking for missing functions we can replace
#
AC_REPLACE_FUNCS(basename)
AC_REPLACE_FUNCS(inet_aton)
AC_REPLACE_FUNCS(inet_pton)
AC_REPLACE_FUNCS(inet_ntop)
AC_REPLACE_FUNCS(snprintf)
AC_REPLACE_FUNCS(strlcat)
AC_REPLACE_FUNCS(strlcpy)
AC_REPLACE_FUNCS(strptime)
AC_REPLACE_FUNCS(pselect)
AC_REPLACE_FUNCS(memmove)
AC_REPLACE_FUNCS(reallocarray)

#
# Check for b64_ntop (and assume result applies to b64_pton as well).
# The functions are actually prefixed with __ and resolv.h defines
# macros for the unprefixed versions, however including this header
# causes conflicts with our T_xx defines.
#
AC_MSG_CHECKING(for __b64_ntop)
AC_TRY_LINK([#include <stddef.h>],
	[__b64_ntop(NULL, 0, NULL, 0);],
	found_b64_ntop=yes,
	found_b64_ntop=no
)
if test "x$found_b64_ntop" = xno; then
	AC_MSG_RESULT(no)

	AC_MSG_CHECKING(for __b64_ntop with -lresolv)
	LIBS="$LIBS -lresolv"
	AC_TRY_LINK([#include <stddef.h>],
		[__b64_ntop(NULL, 0, NULL, 0);],
		found_b64_ntop=yes,
		found_b64_ntop=no
	)
	if test "x$found_b64_ntop" = xno; then
		AC_MSG_RESULT(no)
	fi
fi
if test "x$found_b64_ntop" = xyes; then
	AC_DEFINE(HAVE_B64_NTOP)
	AC_DEFINE(HAVE_B64_PTON)
	AC_MSG_RESULT(yes)
else
	AC_LIBOBJ([b64_ntop])
	AC_LIBOBJ([b64_pton])
fi

AC_MSG_CHECKING(for pselect prototype in sys/select.h)
AC_EGREP_HEADER([[^a-zA-Z_]*pselect[^a-zA-Z_]], sys/select.h, AC_DEFINE(HAVE_PSELECT_PROTO, 1, 
	[if sys/select.h provides pselect prototype]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

AC_MSG_CHECKING(for ctime_r prototype in time.h)
AC_EGREP_HEADER([[^a-zA-Z_]*ctime_r[^a-zA-Z_]], time.h, AC_DEFINE(HAVE_CTIME_R_PROTO, 1, 
	[if time.h provides ctime_r prototype]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

AC_CHECK_TYPE([struct timespec], AC_DEFINE(HAVE_STRUCT_TIMESPEC, 1, [If time.h has a struct timespec (for pselect).]), [], [
AC_INCLUDES_DEFAULT
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
])

dnl
dnl Some random defines's
dnl
AC_DEFINE_UNQUOTED([IDENTITY], ["unidentified server"], [Define to the default nsd identity.])
AC_DEFINE_UNQUOTED([VERSION], [PACKAGE_STRING], [Define to the NSD version to answer version.server query.])
AC_DEFINE_UNQUOTED([TCP_BACKLOG], [256], [Define to the backlog to be used with listen.])
AC_DEFINE_UNQUOTED([TCP_PORT], ["53"], [Define to the default tcp port.])
AC_DEFINE_UNQUOTED([TCP_MAX_MESSAGE_LEN], [65535], [Define to the default maximum message length.])
AC_DEFINE_UNQUOTED([UDP_PORT], ["53"], [Define to the default udp port.])
AC_DEFINE_UNQUOTED([UDP_MAX_MESSAGE_LEN], [512], [Define to the default maximum udp message length.])
AC_DEFINE_UNQUOTED([EDNS_MAX_MESSAGE_LEN], [4096], [Define to the default maximum message length with EDNS.])
AC_DEFINE_UNQUOTED([MAXSYSLOGMSGLEN], [512], [Define to the maximum message length to pass to syslog.])
AC_DEFINE_UNQUOTED([NSD_CONTROL_PORT], [8952], [Define to the default nsd-control port.])
AC_DEFINE_UNQUOTED([NSD_CONTROL_VERSION], [1], [Define to nsd-control proto version.])

dnl
dnl Determine the syslog facility to use
dnl
facility=LOG_DAEMON
AC_ARG_WITH([facility],
        AC_HELP_STRING([--with-facility=name], [Syslog default facility (LOG_DAEMON)]),
        [facility=$withval])
AC_DEFINE_UNQUOTED([FACILITY], $facility, [Define to the default facility for syslog.])

dnl
dnl Determine the default tcp timeout
dnl
tcp_timeout=120
AC_ARG_WITH([tcp_timeout],
	AC_HELP_STRING([--with-tcp-timeout=number], [Limit the default tcp timeout]),
	[tcp_timeout=$withval])
AC_DEFINE_UNQUOTED([TCP_TIMEOUT], $tcp_timeout, [Define to the default tcp timeout.])

dnl
dnl Features
dnl
AC_ARG_ENABLE(root-server, AC_HELP_STRING([--enable-root-server], [Configure NSD as a root server]))
case "$enable_root_server" in
        yes)
		AC_DEFINE_UNQUOTED([ROOT_SERVER], [], [Define this to configure as a root server.])
		;;
        no|*)
                ;;
esac

AC_ARG_ENABLE(ipv6, AC_HELP_STRING([--disable-ipv6], [Disables IPv6 support]))
case "$enable_ipv6" in
        no)
                ;;
        yes|*)
		AC_DEFINE_UNQUOTED([INET6], [], [Define this to enable IPv6 support.])
		;;
esac

AC_ARG_ENABLE(bind8-stats, AC_HELP_STRING([--enable-bind8-stats], [Enables BIND8 like NSTATS & XSTATS and statistics in nsd-control]))

case "$enable_bind8_stats" in
	yes|'')
		AC_DEFINE_UNQUOTED([BIND8_STATS], [], [Define this to enable BIND8 like NSTATS & XSTATS.])
		;;
	no|*)
		;;
esac

AC_ARG_ENABLE(zone-stats, AC_HELP_STRING([--enable-zone-stats], [Enable per-zone statistics gathering (needs --enable-bind8-stats)]))
case "$enable_zone_stats" in
	yes)
		AC_DEFINE_UNQUOTED([USE_ZONE_STATS], [], [Define this to enable per-zone statistics gathering.])
		AC_DEFINE_UNQUOTED([BIND8_STATS], [], [Define this to enable BIND8 like NSTATS & XSTATS.])
		;;
	no|''|*)
		;;
esac

AC_ARG_ENABLE(checking, AC_HELP_STRING([--enable-checking], [Enable internal runtime checks]))
case "$enable_checking" in
        yes)
		CHECK_COMPILER_FLAG(W, [ CFLAGS="$CFLAGS -W" ])
		CHECK_COMPILER_FLAG(Wall, [ CFLAGS="$CFLAGS -Wall" ])
		CHECK_COMPILER_FLAG(Wextra, [ CFLAGS="$CFLAGS -Wextra" ])
		CHECK_COMPILER_FLAG(Wdeclaration-after-statement, [ CFLAGS="$CFLAGS -Wdeclaration-after-statement" ])
		;;
        no|*)
                AC_DEFINE([NDEBUG], [], [Undefine this to enable internal runtime checks.])
                ;;
esac

AC_ARG_ENABLE(ratelimit, AC_HELP_STRING([--enable-ratelimit], [Enable rate limiting]))
case "$enable_ratelimit" in
	yes)
		AC_DEFINE_UNQUOTED([RATELIMIT], [], [Define this to enable rate limiting.])
		dnl causes awk to not match the exclusion start marker.
		ratelimit="xx"
		;;
	no|*)
		ratelimit=""
		;;
esac
AC_SUBST(ratelimit)

AC_ARG_ENABLE(ratelimit-default-is-off, AC_HELP_STRING([--enable-ratelimit-default-is-off], [Enable this to set default of ratelimit to off (enable in nsd.conf), otherwise ratelimit is enabled by default if --enable-ratelimit is enabled]))
case "$enable_ratelimit_default_is_off" in
	yes)
		AC_DEFINE_UNQUOTED([RATELIMIT_DEFAULT_OFF], [], [Define this to set ratelimit to off by default.])
		ratelimit_default="off"
		;;
	no|*)
		ratelimit_default="on"
		;;
esac
AC_SUBST(ratelimit_default)


# we need SSL for TSIG (and maybe also for NSEC3).
CHECK_SSL
if test x$HAVE_SSL = x"yes"; then
	ACX_LIB_SSL

	# Check for -pthread
	BAKLIBS="$LIBS"
	LIBS="-lcrypto $LIBS"
	AC_TRY_LINK([], [
		int HMAC_Update(void);
		(void)HMAC_Update();
	], [],[
		dnl so link fails for HMAC_Update, try with -pthread.
		BAKCFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS -pthread"
		AC_MSG_CHECKING([if libcrypto needs -pthread])
		AC_TRY_LINK_FUNC([HMAC_Update], [
			AC_MSG_RESULT([yes])
		] , [
			AC_MSG_RESULT([no])
			dnl restore the nonpthread value
			CFLAGS="$BAKCFLAGS"
		])
	])
	LIBS="$BAKLIBS"

	if test -n "$ssldir"; then 
            AC_CHECK_LIB(crypto, HMAC_Update,, [
                    AC_MSG_ERROR([OpenSSL found in $ssldir, but version 0.9.7 or higher is required])
                ])
	fi
	SSL_LIBS="-lssl"
	AC_SUBST(SSL_LIBS)
	AC_CHECK_HEADERS([openssl/ssl.h],,, [AC_INCLUDES_DEFAULT])
	AC_CHECK_HEADERS([openssl/err.h],,, [AC_INCLUDES_DEFAULT])
	AC_CHECK_HEADERS([openssl/rand.h],,, [AC_INCLUDES_DEFAULT])
	AC_CHECK_FUNCS([HMAC_CTX_reset HMAC_CTX_new])
else
	AC_MSG_WARN([No SSL, therefore remote-control is disabled])
fi

AC_ARG_ENABLE(nsec3, AC_HELP_STRING([--disable-nsec3], [Disable NSEC3 support]))
case "$enable_nsec3" in
        no)
		;;
        yes)
		AC_DEFINE_UNQUOTED([NSEC3], [], [Define this to enable NSEC3 support.])
		;;
		*)
		if test x$HAVE_SSL = x"yes"; then
			AC_DEFINE_UNQUOTED([NSEC3], [], [Define this to enable NSEC3 support.])
		else
			AC_MSG_WARN([No SSL, therefore NSEC3 is disabled])
		fi
                ;;
esac

AC_ARG_ENABLE(minimal-responses, AC_HELP_STRING([--disable-minimal-responses], [Disable response minimization. More truncation.]))
case "$enable_minimal_responses" in
        no)
		;;
        yes|*)
		AC_DEFINE_UNQUOTED([MINIMAL_RESPONSES], [], [Define this to enable response minimalization to reduce truncation.])
                ;;
esac

AC_ARG_ENABLE(mmap, AC_HELP_STRING([--enable-mmap], [Use mmap instead of malloc. Experimental.]))
case "$enable_mmap" in
        yes)
		AC_CHECK_HEADERS([sys/mman.h])
		AC_LIBGTOP_CHECK_TYPE(uintptr_t, void*)
		AC_CHECK_FUNCS([mmap munmap])
		AC_DEFINE_UNQUOTED([USE_MMAP_ALLOC], [], [Define this to enable mmap instead of malloc. Experimental.])
		;;
        no|*)
                ;;
esac

AH_BOTTOM([
/* define before includes as it specifies what standard to use. */
#if (defined(HAVE_PSELECT) && !defined (HAVE_PSELECT_PROTO)) \
	|| !defined (HAVE_CTIME_R_PROTO) \
	|| defined (STRPTIME_NEEDS_DEFINES)
#  ifndef _XOPEN_SOURCE
#    define _XOPEN_SOURCE 600
#  endif
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200112
#  endif
#  ifndef _BSD_SOURCE
#    define _BSD_SOURCE 1
#  endif
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE 1
#  endif
#  ifndef __EXTENSIONS__
#    define __EXTENSIONS__ 1
#  endif 
#  ifndef _STDC_C99
#    define _STDC_C99 1
#  endif
#  ifndef _ALL_SOURCE
#    define _ALL_SOURCE 1
#  endif
#endif
])

AH_BOTTOM([
#ifdef HAVE_VA_LIST_DOUBLE_DEF
/* workaround double va_list definition on some platforms */
#  ifndef _VA_LIST_DEFINED
#    define _VA_LIST_DEFINED
#  endif
#endif
])

AH_BOTTOM([
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/* For Tru64 */
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
])

AH_BOTTOM([
#ifdef HAVE_ATTR_FORMAT
#define ATTR_FORMAT(archetype, string_index, first_to_check) \
    __attribute__ ((format (archetype, string_index, first_to_check)))
#else /* !HAVE_ATTR_FORMAT */
#define ATTR_FORMAT(archetype, string_index, first_to_check) /* empty */
#endif /* !HAVE_ATTR_FORMAT */
#if defined(__cplusplus)
#define ATTR_UNUSED(x)
#elif defined(HAVE_ATTR_UNUSED)
#define ATTR_UNUSED(x)  x __attribute__((unused))
#else /* !HAVE_ATTR_UNUSED */
#define ATTR_UNUSED(x)  x
#endif /* !HAVE_ATTR_UNUSED */
])

AH_BOTTOM([
#ifndef IPV6_MIN_MTU
#define IPV6_MIN_MTU 1280
#endif /* IPV6_MIN_MTU */

#ifndef AF_INET6
#define AF_INET6	28
#endif /* AF_INET6 */
])

if test $ac_cv_func_getaddrinfo = no; then
AC_LIBOBJ([fake-rfc2553])
fi

AH_BOTTOM([
/* maximum nesting of included files */
#define MAXINCLUDES 10
])

AH_BOTTOM([
int __b64_ntop(uint8_t const *src, size_t srclength,
	     char *target, size_t targsize);
int __b64_pton(char const *src, uint8_t *target, size_t targsize);
])
AH_BOTTOM([
#ifndef HAVE_FSEEKO
#define fseeko fseek
#define ftello ftell
#endif /* HAVE_FSEEKO */
#ifndef HAVE_SNPRINTF
#include <stdarg.h>
int snprintf (char *str, size_t count, const char *fmt, ...);
int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif /* HAVE_SNPRINTF */
#ifndef HAVE_INET_PTON
int inet_pton(int af, const char* src, void* dst);
#endif /* HAVE_INET_PTON */
#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif
#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr);
#endif
#ifndef HAVE_MEMMOVE
void *memmove(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_STRLCAT
size_t strlcat(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_REALLOCARRAY
void* reallocarray(void *ptr, size_t nmemb, size_t size);
#endif
#ifndef HAVE_GETADDRINFO
#include "compat/fake-rfc2553.h"
#endif
#ifndef HAVE_STRPTIME
#define HAVE_STRPTIME 1
char *strptime(const char *s, const char *format, struct tm *tm); 
#endif
#ifndef STRPTIME_WORKS
#define STRPTIME_WORKS 1
char *nsd_strptime(const char *s, const char *format, struct tm *tm); 
#define strptime(a,b,c) nsd_strptime((a),(b),(c))
#endif
])
AH_BOTTOM(
AHX_MEMCMP_BROKEN(nsd)
AHX_CONFIG_MAXHOSTNAMELEN
)
AH_BOTTOM([

/* provide timespec def if not available */
#ifndef CONFIG_DEFINES
#define CONFIG_DEFINES
#ifndef HAVE_STRUCT_TIMESPEC
#ifndef __timespec_defined
#define __timespec_defined 1
	struct timespec {
		long    tv_sec;         /* seconds */
		long    tv_nsec;        /* nanoseconds */
	};
#endif /* !__timespec_defined */
#endif /* !HAVE_STRUCT_TIMESPEC */
#endif /* !CONFIG_DEFINES */
])

# big fat warning
if test "$enable_checking" = "yes"; then
        echo "************************************************"
        echo "* You have activated \"--enable-checking\"       *"
        echo "*                                              *"
        echo "* This will instruct NSD to be stricter        *"
        echo "* when validating its input. This could lead   *"
        echo "* to a reduced service level.                  *"
        echo "*                                              *"
        echo "************************************************"
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

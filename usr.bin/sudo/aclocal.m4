dnl Local m4 macors for autoconf (used by sudo)
dnl
dnl Copyright (c) 1994-1996,1998-2003 Todd C. Miller <Todd.Miller@courtesan.com>
dnl
dnl XXX - should cache values in all cases!!!
dnl
dnl checks for programs

dnl
dnl check for sendmail
dnl
AC_DEFUN(SUDO_PROG_SENDMAIL, [AC_MSG_CHECKING(for sendmail)
if test -f "/usr/sbin/sendmail"; then
    AC_MSG_RESULT(/usr/sbin/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/sbin/sendmail")
elif test -f "/usr/lib/sendmail"; then
    AC_MSG_RESULT(/usr/lib/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/lib/sendmail")
elif test -f "/usr/etc/sendmail"; then
    AC_MSG_RESULT(/usr/etc/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/etc/sendmail")
elif test -f "/usr/ucblib/sendmail"; then
    AC_MSG_RESULT(/usr/ucblib/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/ucblib/sendmail")
elif test -f "/usr/local/lib/sendmail"; then
    AC_MSG_RESULT(/usr/local/lib/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/local/lib/sendmail")
elif test -f "/usr/local/bin/sendmail"; then
    AC_MSG_RESULT(/usr/local/bin/sendmail)
    SUDO_DEFINE(_PATH_SUDO_SENDMAIL, "/usr/local/bin/sendmail")
else
    AC_MSG_RESULT(not found)
fi
])dnl

dnl
dnl check for vi
dnl
AC_DEFUN(SUDO_PROG_VI, [AC_MSG_CHECKING(for vi)
if test -f "/usr/bin/vi"; then
    AC_MSG_RESULT(/usr/bin/vi)
    SUDO_DEFINE(_PATH_VI, "/usr/bin/vi")
elif test -f "/usr/ucb/vi"; then
    AC_MSG_RESULT(/usr/ucb/vi)
    SUDO_DEFINE(_PATH_VI, "/usr/ucb/vi")
elif test -f "/usr/bsd/vi"; then
    AC_MSG_RESULT(/usr/bsd/vi)
    SUDO_DEFINE(_PATH_VI, "/usr/bsd/vi")
elif test -f "/bin/vi"; then
    AC_MSG_RESULT(/bin/vi)
    SUDO_DEFINE(_PATH_VI, "/bin/vi")
elif test -f "/usr/local/bin/vi"; then
    AC_MSG_RESULT(/usr/local/bin/vi)
    SUDO_DEFINE(_PATH_VI, "/usr/local/bin/vi")
else
    AC_MSG_RESULT(not found)
fi
])dnl

dnl
dnl check for mv
dnl
AC_DEFUN(SUDO_PROG_MV, [AC_MSG_CHECKING(for mv)
if test -f "/usr/bin/mv"; then
    AC_MSG_RESULT(/usr/bin/mv)
    SUDO_DEFINE(_PATH_MV, "/usr/bin/mv")
elif test -f "/bin/mv"; then
    AC_MSG_RESULT(/bin/mv)
    SUDO_DEFINE(_PATH_MV, "/bin/mv")
elif test -f "/usr/ucb/mv"; then
    AC_MSG_RESULT(/usr/ucb/mv)
    SUDO_DEFINE(_PATH_MV, "/usr/ucb/mv")
elif test -f "/usr/sbin/mv"; then
    AC_MSG_RESULT(/usr/sbin/mv)
    SUDO_DEFINE(_PATH_MV, "/usr/sbin/mv")
else
    AC_MSG_RESULT(not found)
fi
])dnl

dnl
dnl check for bourne shell
dnl
AC_DEFUN(SUDO_PROG_BSHELL, [AC_MSG_CHECKING(for bourne shell)
if test -f "/bin/sh"; then
    AC_MSG_RESULT(/bin/sh)
    SUDO_DEFINE(_PATH_BSHELL, "/bin/sh")
elif test -f "/usr/bin/sh"; then
    AC_MSG_RESULT(/usr/bin/sh)
    SUDO_DEFINE(_PATH_BSHELL, "/usr/bin/sh")
elif test -f "/sbin/sh"; then
    AC_MSG_RESULT(/sbin/sh)
    SUDO_DEFINE(_PATH_BSHELL, "/sbin/sh")
elif test -f "/usr/sbin/sh"; then
    AC_MSG_RESULT(/usr/sbin/sh)
    SUDO_DEFINE(_PATH_BSHELL, "/usr/sbin/sh")
elif test -f "/bin/ksh"; then
    AC_MSG_RESULT(/bin/ksh)
    SUDO_DEFINE(_PATH_BSHELL, "/bin/ksh")
elif test -f "/usr/bin/ksh"; then
    AC_MSG_RESULT(/usr/bin/ksh)
    SUDO_DEFINE(_PATH_BSHELL, "/usr/bin/ksh")
elif test -f "/bin/bash"; then
    AC_MSG_RESULT(/bin/bash)
    SUDO_DEFINE(_PATH_BSHELL, "/bin/bash")
elif test -f "/usr/bin/bash"; then
    AC_MSG_RESULT(/usr/bin/bash)
    SUDO_DEFINE(_PATH_BSHELL, "/usr/bin/bash")
else
    AC_MSG_RESULT(not found)
fi
])dnl

dnl
dnl Where the log file goes, use /var/log if it exists, else /{var,usr}/adm
dnl
AC_DEFUN(SUDO_LOGFILE, [AC_MSG_CHECKING(for log file location)
if test -n "$with_logpath"; then
    AC_MSG_RESULT($with_logpath)
    SUDO_DEFINE_UNQUOTED(_PATH_SUDO_LOGFILE, "$with_logpath")
elif test -d "/var/log"; then
    AC_MSG_RESULT(/var/log/sudo.log)
    SUDO_DEFINE(_PATH_SUDO_LOGFILE, "/var/log/sudo.log")
elif test -d "/var/adm"; then
    AC_MSG_RESULT(/var/adm/sudo.log)
    SUDO_DEFINE(_PATH_SUDO_LOGFILE, "/var/adm/sudo.log")
elif test -d "/usr/adm"; then
    AC_MSG_RESULT(/usr/adm/sudo.log)
    SUDO_DEFINE(_PATH_SUDO_LOGFILE, "/usr/adm/sudo.log")
else
    AC_MSG_RESULT(unknown, you will have to set _PATH_SUDO_LOGFILE by hand)
fi
])dnl

dnl
dnl Where the log file goes, use /var/log if it exists, else /{var,usr}/adm
dnl
AC_DEFUN(SUDO_TIMEDIR, [AC_MSG_CHECKING(for timestamp file location)
if test -n "$with_timedir"; then
    AC_MSG_RESULT($with_timedir)
    SUDO_DEFINE_UNQUOTED(_PATH_SUDO_TIMEDIR, "$with_timedir")
    timedir="$with_timedir"
elif test -d "/var/run"; then
    AC_MSG_RESULT(/var/run/sudo)
    SUDO_DEFINE(_PATH_SUDO_TIMEDIR, "/var/run/sudo")
    timedir="/var/run/sudo"
elif test -d "/var/adm"; then
    AC_MSG_RESULT(/var/adm/sudo)
    SUDO_DEFINE(_PATH_SUDO_TIMEDIR, "/var/adm/sudo")
    timedir="/var/adm/sudo"
else
    AC_MSG_RESULT(/usr/adm/sudo)
    SUDO_DEFINE(_PATH_SUDO_TIMEDIR, "/usr/adm/sudo")
    timedir="/usr/adm/sudo"
fi
])dnl

dnl
dnl check for fullly working void
dnl
AC_DEFUN(SUDO_FULL_VOID, [AC_MSG_CHECKING(for full void implementation)
AC_TRY_COMPILE(, [void *foo;
foo = (void *)0; (void *)"test";], AC_DEFINE(VOID, void, [Define to "void" if your compiler supports void pointers, else use "char"].)
AC_MSG_RESULT(yes), AC_DEFINE(VOID, char)
AC_MSG_RESULT(no))])

dnl
dnl SUDO_CHECK_TYPE(TYPE, DEFAULT)
dnl XXX - should require the check for unistd.h...
dnl
AC_DEFUN(SUDO_CHECK_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(sudo_cv_type_$1,
[AC_EGREP_CPP($1, [#include <sys/types.h>
#include <stdio.h>
#if STDC_HEADERS
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif], sudo_cv_type_$1=yes, sudo_cv_type_$1=no)])dnl
AC_MSG_RESULT($sudo_cv_type_$1)
if test $sudo_cv_type_$1 = no; then
  AC_DEFINE($1, $2, [Define if your system lacks the $1 type.])
fi
])

dnl
dnl Check for size_t declation
dnl
AC_DEFUN(SUDO_TYPE_SIZE_T,
[SUDO_CHECK_TYPE(size_t, int)])

dnl
dnl Check for ssize_t declation
dnl
AC_DEFUN(SUDO_TYPE_SSIZE_T,
[SUDO_CHECK_TYPE(ssize_t, int)])

dnl
dnl Check for dev_t declation
dnl
AC_DEFUN(SUDO_TYPE_DEV_T,
[SUDO_CHECK_TYPE(dev_t, int)])

dnl
dnl Check for ino_t declation
dnl
AC_DEFUN(SUDO_TYPE_INO_T,
[SUDO_CHECK_TYPE(ino_t, unsigned int)])

dnl
dnl check for working fnmatch(3)
dnl
AC_DEFUN(SUDO_FUNC_FNMATCH,
[AC_MSG_CHECKING(for working fnmatch with FNM_CASEFOLD)
AC_CACHE_VAL(sudo_cv_func_fnmatch,
[rm -f conftestdata; > conftestdata
AC_TRY_RUN([#include <fnmatch.h>
main() { exit(fnmatch("/*/bin/echo *", "/usr/bin/echo just a test", FNM_CASEFOLD)); }
], sudo_cv_func_fnmatch=yes, sudo_cv_func_fnmatch=no,
  sudo_cv_func_fnmatch=no)
rm -f core core.* *.core])dnl
AC_MSG_RESULT($sudo_cv_func_fnmatch)
if test $sudo_cv_func_fnmatch = yes; then
  [$1]
else
  [$2]
fi
])

dnl
dnl check for isblank(3)
dnl
AC_DEFUN([SUDO_FUNC_ISBLANK],
  [AC_CACHE_CHECK([for isblank], sudo_cv_func_isblank,
    [AC_TRY_LINK([#include <ctype.h>], [(void)isblank(1);],
    sudo_cv_func_isblank=yes, sudo_cv_func_isblank=no)])
] [
  if test "$sudo_cv_func_isblank" = "yes"; then
    AC_DEFINE(HAVE_ISBLANK, 1, [Define if you have isblank(3).])
  fi
])

dnl
dnl check for sa_len field in struct sockaddr
dnl
AC_DEFUN(SUDO_SOCK_SA_LEN,
[AC_MSG_CHECKING(for sa_len field in struct sockaddr)
AC_CACHE_VAL(sudo_cv_sock_sa_len,
[AC_TRY_RUN([#include <sys/types.h>
#include <sys/socket.h>
main() {
struct sockaddr s;
s.sa_len = 0;
exit(0);
}], sudo_cv_sock_sa_len=yes, sudo_cv_sock_sa_len=no,
  sudo_cv_sock_sa_len=no)
rm -f core core.* *.core])dnl
AC_MSG_RESULT($sudo_cv_sock_sa_len)
if test $sudo_cv_sock_sa_len = yes; then
  AC_DEFINE(HAVE_SA_LEN, 1, [Define if your struct sockadr has an sa_len field.])
fi
])

dnl
dnl check for max length of uid_t in string representation.
dnl we can't really trust UID_MAX or MAXUID since they may exist
dnl only for backwards compatibility.
dnl
AC_DEFUN(SUDO_UID_T_LEN,
[AC_REQUIRE([AC_TYPE_UID_T])
AC_MSG_CHECKING(max length of uid_t)
AC_CACHE_VAL(sudo_cv_uid_t_len,
[rm -f conftestdata
AC_TRY_RUN(
[#include <stdio.h>
#include <pwd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/param.h>
main() {
  FILE *f;
  char b[1024];
  uid_t u = (uid_t) -1;

  if ((f = fopen("conftestdata", "w")) == NULL)
    exit(1);

  (void) sprintf(b, "%lu", (unsigned long) u);
  (void) fprintf(f, "%d\n", strlen(b));
  (void) fclose(f);
  exit(0);
}], sudo_cv_uid_t_len=`cat conftestdata`, sudo_cv_uid_t_len=10)
])
rm -f conftestdata
AC_MSG_RESULT($sudo_cv_uid_t_len)
AC_DEFINE_UNQUOTED(MAX_UID_T_LEN, $sudo_cv_uid_t_len, [Define to the max length of a uid_t in string context (excluding the NUL).])
])

dnl
dnl check for "long long"
dnl XXX hard to cache since it includes 2 tests
dnl
AC_DEFUN(SUDO_LONG_LONG, [AC_MSG_CHECKING(for long long support)
AC_TRY_LINK(, [long long foo = 1000; foo /= 10;], AC_DEFINE(HAVE_LONG_LONG, 1, [Define if your compiler supports the "long long" type.])
[AC_TRY_RUN([main() {if (sizeof(long long) == sizeof(long)) exit(0); else exit(1);}], AC_DEFINE(LONG_IS_QUAD, 1, [Define if sizeof(long) == sizeof(long long).]))]
AC_MSG_RESULT(yes), AC_MSG_RESULT(no))])

dnl
dnl append a libpath to an LDFLAGS style variable
dnl
AC_DEFUN(SUDO_APPEND_LIBPATH, [
    if test X"$with_rpath" = X"yes"; then
	$1="${$1} -L$2 -R$2"
    else
	$1="${$1} -L$2"
    fi
    if test X"$blibpath" != X"" -a "$1" = "SUDO_LDFLAGS"; then
	blibpath_add="${blibpath_add}:$2"
    fi
])

dnl
dnl private versions of AC_DEFINE and AC_DEFINE_UNQUOTED that don't support
dnl tracing that we use to define paths for pathnames.h so autoheader doesn't
dnl put them in config.h.in.  An awful hack.
dnl
m4_define([SUDO_DEFINE],
[cat >>confdefs.h <<\EOF
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
EOF
])

m4_define([SUDO_DEFINE_UNQUOTED],
[cat >>confdefs.h <<EOF
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
EOF
])

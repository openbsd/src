define([AC_FIND_PROGRAM],dnl
[if test x$3 = x; then _PATH=$PATH; else _PATH=$3; fi
if test -z "[$]$1"; then
  # Extract the first word of `$2', so it can be a program name with args.
  set dummy $2; word=[$]2
  echo checking for $word
  IFS="${IFS= 	}"; saveifs="$IFS"; IFS="${IFS}:"
  for dir in $_PATH; do
    test -z "$dir" && dir=.
    if test -f $dir/$word; then
      $1=$dir/$word
      break
    fi
  done
  IFS="$saveifs"
fi
test -n "[$]$1" && test -n "$verbose" && echo "	setting $1 to [$]$1"
AC_SUBST($1)dnl
])dnl
dnl
define([AC_ECHON],dnl
[echo checking for echo -n
if test "`echo -n foo`" = foo ; then
  ECHON=bsd
  test -n "$verbose" && echo '	using echo -n'
elif test "`echo 'foo\c'`" = foo ; then
  ECHON=sysv
  test -n "$verbose" && echo '	using echo ...\\c'
else
  ECHON=none
  test -n "$verbose" && echo '	using plain echo'
fi])dnl
dnl
define([AC_LISPDIR],dnl
[AC_MSG_CHECKING(checking for Emacs Lisp files)
if test -n "$with_lispdir"; then
  LISPDIR=${with_lispdir}
else
  for f in ${prefix-/usr/local}/lib/emacs/site-lisp \
	   ${prefix-/usr/local}/lib/emacs/lisp; do
    if test -d $f; then
      if test -n "$prefix"; then
	LISPDIR=`echo $f | sed "s,^$prefix,"'$(prefix),'`
      else
	LISPDIR=$f
      fi
      break
    fi
  done
fi
if test -z "$LISPDIR"; then
dnl # Change this default when Emacs 19 has been around for a while
  LISPDIR='$(prefix)/lib/emacs/lisp'
fi
AC_MSG_RESULT(${LISPDIR})
AC_SUBST(LISPDIR)dnl
])dnl
dnl
define([AC_PASSWD],dnl
[echo checking how to access passwd database
PASSWD="cat /etc/passwd"
if test -f /bin/domainname && test -n "`/bin/domainname`"; then
  if test -f /usr/bin/niscat && 
     /usr/bin/niscat passwd.org_dir > /dev/null 2>&1; then
    PASSWD="/usr/bin/niscat passwd.org_dir"
  elif test -f /usr/bin/ypcat && /usr/bin/ypcat passwd > /dev/null 2>&1; then
    PASSWD="/usr/bin/ypcat passwd"
  fi
fi
test -n "$verbose" && echo "	setting PASSWD to ${PASSWD}"
AC_SUBST(PASSWD)dnl
])dnl

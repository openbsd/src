dnl
dnl $KTH: check-roken.m4,v 1.1.2.1 2002/02/01 14:18:23 ahltorp Exp $
dnl

AC_DEFUN(AC_CHECK_ROKEN,[

ROKEN_H=roken.h
DIR_roken=roken
LIB_roken='$(top_builddir)/lib/roken/libroken.a'
INC_roken='-I$(top_builddir)/include'

AC_ARG_WITH(roken,
[  --with-roken=dir        make with roken in dir],
[if test "$with_roken" != "no"; then
   ROKEN_H=
   DIR_roken=
   if test "X$withval" != "X"; then
   	LIB_roken="$withval/lib/libroken.a"
   	INC_roken="-I$withval/include"
   else
	LIB_roken='-lroken'
	INC_roken=
   fi
fi])

AC_ARG_WITH(roken-include,
[ --roken-include=dir      make with roken headers in dir],
[if test "$with_roken" != "no"; then
   ROKEN_H=
   DIR_roken=
   if test "X$withval" != "X"; then
   	INC_roken="-I$withval"
   else
	INC_roken=
   fi
fi])

AC_ARG_WITH(roken-lib,
[  --roken-lib=dir         make with roken lib in dir],
[if test "$with_roken" != "no"; then
   ROKEN_H=
   DIR_roken=
   if test "X$withval" != "X"; then
   	LIB_roken="$withval/libroken.a"
   else
   	LIB_roken="-lroken"
   fi
fi])

if test "X$ROKEN_H" = "X"; then

AC_CACHE_CHECK([check what roken depends on ],[ac_cv_roken_deps],[
ac_cv_roken_deps="error"
saved_LIBS="$LIBS"
for a in "" "-lcrypt" ; do
  LIBS="$saved_LIBS $LIB_roken $a"
  AC_TRY_LINK([],[getarg()],[
    if test "X$a" = "X"; then
      ac_cv_roken_deps="nothing"
    else
      ac_cv_roken_deps="$a"
    fi],[])
  LIBS="$saved_LIBS"
  if test $ac_cv_roken_deps != "error"; then break; fi
done
LIBS="$saved_LIBS"
])

if test "$ac_cv_roken_deps" = "error"; then
  AC_MSG_ERROR([failed to figure out libroken depencies])
fi

if test "$ac_cv_roken_deps" != "nothing"; then
  LIB_roken="$LIB_roken $ac_cv_roken_deps"
fi

fi

AC_SUBST(INC_roken)
AC_SUBST(LIB_roken)
AC_SUBST(DIR_roken)
AC_SUBST(ROKEN_H)

])

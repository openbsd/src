dnl
dnl $KTH: check-kerberos.m4,v 1.38.2.2 2001/10/23 23:27:23 ahltorp Exp $
dnl
dnl Check if the dog is alive
dnl
dnl Arguments:
dnl
dnl   AC_CHECK_KERBEROS(4):
dnl      check for krb4 libs and krb5 krb4 compat libs
dnl   AC_CHECK_KERBEROS(45):
dnl      check for krb4 libs and krb5 libs
dnl   AC_CHECK_KERBEROS(5):
dnl      check for krb5 libs
dnl   AC_CHECK_KERBEROS([45]auto):
dnl      check for kerberos without a --with-krbN
dnl
dnl Set:
dnl   ac_cv_found_krb4=[yes|no]
dnl   KRB4_{LIB|INC}_DIR
dnl   KRB4_{LIB,INC}_FLAGS
dnl   KRB4_LIB_LIBS
dnl
dnl   ac_cv_found_krb5=[yes|no]
dnl   KRB5_{LIB|INC}_DIR
dnl   KRB5_{LIB,INC}_FLAGS
dnl   KRB5_LIB_LIBS
dnl
dnl
dnl Check order
dnl    krb5
dnl     include testing of libk5crypto/libcrypto
dnl    krb4
dnl      krb5 compat if needed (included in krb4 test)
dnl      this is the reson krb5 is tested first
dnl

AC_DEFUN(AC_CHECK_KERBEROS,[

dnl
dnl Parse args for kerberos4
dnl

ifelse(-1,regexp($1,[[45]]),[
  errprint(__file__:__line__: [CHECK_KERBEROS: have to give at lease 4 or 5
])
  m4exit(1)
])

ifelse(-1,regexp($1,4),[with_krb4=no],[ dnl WITHOUT KERBEROS4 (1)

AC_ARG_WITH(krb4,
[  --with-krb4=dir         use kerberos 4 in dir],
[if test "X$withval" = "X"; then
  ifelse(-1,regexp($1,[auto]),[
    AC_MSG_ERROR([You have to give a dir to --with-krb4])],
    [:])
fi],
[ifelse(-1,regexp($1,auto),[with_krb4=no],[with_krb4=yes])])

AC_ARG_WITH(krb4-lib,
[  --with-krb4-lib=dir     use kerberos 4 libraries in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-krb4-lib])
fi])

AC_ARG_WITH(krb4-include,
[  --with-krb4-include=dir use kerberos 4 headers in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-krb4-include])
fi])

]) dnl WITHOUT KERBEROS4 (1)

dnl
dnl Parse args for kerberos5
dnl

AC_ARG_WITH(krb5,
[  --with-krb5=dir         use ]dnl
ifelse(-1,regexp($1,5),[kerberos 4 compat from ])[kerberos 5 in dir],
[
if test "X$withval" = "X"; then
  ifelse(-1,regexp($1,[auto]),[
    AC_MSG_ERROR([You have to give a dir to --with-krb5])],
    [:])
  ifelse(-1,regexp($1,5),[
    if test "X$with_krb4" != "Xno"; then
       AC_MSG_ERROR([Already specified kerberos 4 with --with-krb4])
    fi])
elif test "$withval" = "no"; then
ifelse(-1,regexp($1,4),[with_krb5=no],
[if test "$with_krb4" = "yes"; then
  with_krb5=compat
else
  with_krb5=no
fi])
fi
],
[ifelse(-1,regexp($1,auto),[with_krb5=compat],[with_krb5=yes])])

AC_ARG_WITH(krb5-include,
[  --with-krb5-include=dir use ]dnl
ifelse(-1,regexp($1,5),[kerberos 4 compat from ])[kerberos 5 headers in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-krb5-include])
fi
ifelse(-1,regexp($1,5),[
if test "X$with_krb4_include" != "X"; then
  AC_MSG_ERROR([Already specified kerberos 4 headers with --with-krb4-include])
fi])])

AC_ARG_WITH(krb5-lib,
[  --with-krb5-lib=dir     use kerberos 5 libraries in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-krb5-lib])
fi
ifelse(-1,regexp($1,5),[
if test "X$with_krb4_lib" != "X"; then
  AC_MSG_ERROR([Already specified kerberos 4 libraries with --with-krb4-lib])
fi])])


dnl
dnl Check for kerberos5
dnl

case X"$with_krb5" in
  Xyes|Xno|Xcompat)
    ;;
  *)
    ac_cv_krb5_where_lib="$with_krb5/lib"
    ac_cv_krb5_where_inc="$with_krb5/include"
    ac_cv_found_krb5=yes
    ;;
esac

if test "X$with_krb5_include" != "X"; then
  ac_cv_krb4_where_inc=$with_krb5_include
  ac_cv_found_krb5=yes
fi

if test "X$with_krb5_lib" != "X"; then
  ac_cv_krb5_where_lib=$with_krb5_lib
  with_krb5=yes
fi

ifelse(-1,regexp($1,auto),[],[
if test "$with_krb5" != "no"; then
  if test "$ac_cv_krb5_where_lib" = ""; then
    AC_KRB5_LIB_WHERE(["" /usr/heimdal/lib /usr/athena/lib /usr/kerberos/lib /usr/local/lib])
  else
    AC_KRB5_LIB_WHERE($ac_cv_krb5_where_lib)
  fi
fi
])

if test "X$with_krb5_include" != "X"; then
  ac_cv_krb5_where_inc=$with_krb5_include
  ac_cv_found_krb5=yes
  with_krb5=yes
fi

ifelse(-1,regexp($1,auto),[],[ dnl KRB5-AUTO
if test "$with_krb5" != "no"; then
  if test "$ac_cv_krb5_where_inc" = ""; then
    AC_KRB5_INC_WHERE(["" /usr/heimdal/include /usr/athena/include /usr/kerberos/include /usr/include/kerberosV /usr/include/kerberos /usr/include/krb5 /usr/local/include])
  else
    AC_KRB5_INC_WHERE($ac_cv_krb5_where_inc)
  fi
fi
]) dnl KRB5-AUTO

ifelse(-1,regexp($1,4),[],[  dnl WITHOUT KERBEROS4 (2)

dnl
dnl Check for kerberos4
dnl

if test "X$with_krb4" != "Xyes" -a "X$with_krb4" != "Xno"; then
  ac_cv_krb4_where_lib=$with_krb4/lib
  ac_cv_krb4_where_inc=$with_krb4/include
  ac_cv_found_krb4=yes
  ifelse(-1,regexp($1,5),[ac_cv_found_krb5=no])
fi

if test "X$with_krb4_lib" != "X"; then
  ac_cv_krb4_where_lib=$with_krb4_lib
  ac_cv_found_krb4=yes
fi

ifelse(-1,regexp($1,auto),[],[
if test "$with_krb4" != "no"; then
  if test "$ac_cv_krb4_where_lib" = ""; then
    AC_KRB4_LIB_WHERE(["" /usr/athena/lib /usr/kerberos/lib /usr/lib /usr/local/lib])
  else
    AC_KRB4_LIB_WHERE($ac_cv_krb4_where_lib)
  fi
fi
])

if test "X$with_krb4_include" != "X"; then
  ac_cv_krb4_where_inc=$with_krb4_include
  ac_cv_found_krb4=yes
fi

ifelse(-1,regexp($1,auto),[],[
if test "$with_krb4" != "no"; then
  if test "$ac_cv_krb4_where_inc" = ""; then
    AC_KRB4_INC_WHERE(["" /usr/include /usr/athena/include /usr/kerberos/include /usr/include/kerberos /usr/local/include])
  else
    AC_KRB4_INC_WHERE($ac_cv_krb4_where_inc)
  fi
fi
])

]) dnl WITHOUT KERBEROS4 (2)


dnl
dnl
dnl

ifelse(-1,regexp($1,5),[],[   dnl KRB5 VARS

if test "X$ac_cv_found_krb5" = "Xyes"; then
  KRB5_INC_DIR=$ac_cv_krb5_where_inc
  KRB5_LIB_DIR=$ac_cv_krb5_where_lib
  KRB5_INC_FLAGS=
  if test "X$KRB5_INC_DIR" != "X" ; then
    KRB5_INC_FLAGS="-I${KRB5_INC_DIR}"
  fi
  if test "X$KRB5_LIB_DIR" != "X" ; then
     KRB5_LIB_DIR="-L$KRB5_LIB_DIR"
  fi
  KRB5_LIB_LIBS="-lkrb5 -lcom_err $ac_cv_krb5_extralib"
  KRB5_LIB_FLAGS="$KRB5_LIB_DIR $KRB5_LIB_LIBS"
fi

]) dnl KRB5 VARS

ifelse(-1,regexp($1,4),[],[   dnl KRB4 VARS

if test "X$ac_cv_found_krb4" = "Xyes"; then
  KRB4_INC_DIR=$ac_cv_krb4_where_inc
  KRB4_LIB_DIR=$ac_cv_krb4_where_lib
  KRB4_INC_FLAGS=
  if test "X$KRB4_INC_DIR" != "X" ; then
    KRB4_INC_FLAGS="-I${KRB4_INC_DIR}"
  fi
  KRB4_LIB_LIBS="$ac_cv_krb4_extralib"
  if test "X$KRB4_LIB_DIR" != "X" ; then
     KRB4_LIB_DIR="-L${KRB4_LIB_DIR}"
  fi
  KRB4_LIB_FLAGS="$KRB4_LIB_DIR $KRB4_LIB_LIBS"
fi

]) dnl KRB4 VARS


dnl
dnl Export variables, but only we have want it
dnl

dnl KRB 5 - export

if test "X$ac_cv_found_krb5" = "Xyes"; then
  if test "X$ac_cv_found_krb4_lib" = "Xcompat"; then
    ac_cv_found_krb4=yes
    with_krb4=yes
    ifelse(-1,regexp($1,5),[ dnl 5
      ac_cv_found_krb5=no
    ],[
      AC_DEFINE(HAVE_KRB5_COMPAT_KRB4, 1,
        [define if you have kerberos 4 compat])
    ]) dnl END 5
  fi
  ifelse(-1,regexp($1,5),[],[
    with_krb5=yes
    AC_DEFINE(HAVE_KRB5, 1, [define if you have kerberos 5])
  ])
  AC_DEFINE(KERBEROS, 1, [define if you have kerberos])
fi

ifelse(-1,regexp($1,5),[],[
AC_SUBST(KRB5_LIB_DIR)
AC_SUBST(KRB5_INC_DIR)
AC_SUBST(KRB5_INC_FLAGS)
AC_SUBST(KRB5_LIB_LIBS)
AC_SUBST(KRB5_LIB_FLAGS)]) dnl END KRB5 - export

dnl KRB 4 - export

ifelse(-1,regexp($1,4),[],[

if test "$ac_cv_found_krb4" = "yes"; then
  with_krb4=yes
  AC_DEFINE(KERBEROS, 1, [define if you have kerberos])
  AC_DEFINE(HAVE_KRB4, 1, [define if you have kerberos 4])
fi

AC_SUBST(KRB4_LIB_DIR)
AC_SUBST(KRB4_INC_DIR)
AC_SUBST(KRB4_INC_FLAGS)
AC_SUBST(KRB4_LIB_LIBS)
AC_SUBST(KRB4_LIB_FLAGS)]) dnl END KRB4 - export

]) dnl END AC_CHECK_KERBEROS end of this short function

dnl
dnl KRB4 tests
dnl

AC_DEFUN(AC_KRB4_INC_WHERE1, [
saved_CPPFLAGS="${CPPFLAGS}"
if test -n "$1"; then
   CPPFLAGS="$saved_CPPFLAGS -I$1"
fi
AC_TRY_COMPILE([#include <krb.h>],
[struct ktext foo;],
[ac_cv_found_krb4=yes
ac_cv_found_krb4_inc=yes],
ac_cv_found_krb4_inc=no)
CPPFLAGS=$saved_CPPFLAGS
])

AC_DEFUN(AC_KRB4_INC_WHERE, [
   for i in $1; do
      for j in "" kerberos "kerberosIV"; do
	if test -n "$i"; then
	  if test -n "$j"; then
	    d="$i/$j"
	  else
	    d="$i"
	  fi
	else
	  d="$j"
	fi
        AC_MSG_CHECKING(for kerberos4 headers in $d)
	AC_KRB4_INC_WHERE1($d)
        if test "$ac_cv_found_krb4_inc" = "yes"; then
          ac_cv_krb4_where_inc=$d
	  AC_MSG_RESULT(found)
	  break 2
        else
	  AC_MSG_RESULT(not found)
        fi
      done
   done
])

dnl
dnl
dnl


AC_DEFUN(AC_KRB4_LIB_WHERE1, [
saved_LIBS=$LIBS
ac_cv_found_krb4_lib=no
for a in "-lkrb -ldes"			dnl kth-krb && mit-krb
	 "-lkrb -ldes $LIB_roken"		dnl kth-krb in nbsd
	 "-lkrb -ldes $LIB_roken -lcom_err" dnl kth-krb in nbsd for real
	 "-lkrb -ldes -lcom_err" 	dnl kth-krb in fbsd
	 "-lkrb -ldes -lcom_err -lcrypt" dnl CNS q96 à la SCS
	 "-lkrb -lcrypto" dnl kth-krb with openssl
	; do
  LIBS="$saved_LIBS"
  if test "X$1" != "X"; then
     LIBS="$LIBS -L$1"
  fi
  LIBS="$LIBS $a"
  AC_TRY_LINK([],
  [dest_tkt();],
  [ac_cv_found_krb4_lib=yes
   ac_cv_krb4_extralib="$a"
   ac_cv_found_krb4=yes
   break])
done

   if test "$ac_cv_found_krb4_lib" = "no"; then
  for a in "-lkrb4 -ldes425 -lkrb5 -lcom_err $ac_cv_krb5_extralib" dnl
	   "-lkrb4 -ldes524 -lkrb5 -lcom_err $ac_cv_krb5_extralib" dnl
	; do
    LIBS="$saved_LIBS"
    if test "X$1" != "X"; then
      LIBS="$LIBS -L$1"
    fi
    LIBS="$LIBS $a"
    AC_TRY_LINK([], 
    [dest_tkt();],
    [ac_cv_found_krb4_lib=compat
     ac_cv_krb4_extralib="$a"
     break])
done
fi
LIBS=$saved_LIBS
])

AC_DEFUN(AC_KRB4_LIB_WHERE, [
   for i in $1; do
      AC_MSG_CHECKING(for kerberos4 libraries in $i)
      AC_KRB4_LIB_WHERE1($i)
      if test "$ac_cv_found_krb4_lib" != "no" ; then
        ac_cv_krb4_where_lib=$i
	AC_MSG_RESULT(found)
	break
      else
	AC_MSG_RESULT(not found)
      fi
    done
])

dnl
dnl KRB5 tests
dnl

AC_DEFUN(AC_KRB5_INC_WHERE1, [
saved_CPPFLAGS=$CPPFLAGS
if test "X$1" != "X" ; then
   CPPFLAGS="$saved_CPPFLAGS -I$1"
fi
AC_TRY_COMPILE([#include <krb5.h>],
[krb5_kvno foo;],
ac_cv_found_krb5_inc=yes,
ac_cv_found_krb5_inc=no)
CPPFLAGS=$saved_CPPFLAGS
])

AC_DEFUN(AC_KRB5_INC_WHERE, [
   for i in $1; do
      AC_MSG_CHECKING(for kerberos5 headers in $i)
      AC_KRB5_INC_WHERE1($i)
      if test "$ac_cv_found_krb5_inc" = "yes"; then
        ac_cv_krb5_where_inc=$i
	AC_MSG_RESULT(found)
	break
      else
	AC_MSG_RESULT(not found)
      fi
    done
])


dnl
dnl
dnl

AC_DEFUN(AC_KRB5_LIB_WHERE1, [
saved_LIBS=$LIBS
for a in "-lk5crypto -lcom_err" 		dnl new mit krb
	 "-lcrypto -lcom_err" 			dnl old mit krb
         "-lasn1 -ldes $LIB_roken" 		dnl heimdal w/ roken w/o dep on db
         "-lasn1 -ldes $LIB_roken -lresolv" 	dnl heimdal w/ roken w/o dep on db w/ dep on resolv
         "-lasn1 -lcrypto -lcom_err $LIB_roken"	dnl heimdal-BSD w/ roken w/o dep on db
	 "-lasn1 -ldes $LIB_roken -ldb" 		dnl heimdal w/ roken w/ dep on db
	 "-lasn1 -ldes $LIB_roken -ldb -lresolv" 	dnl heimdal w/ roken w/ dep on db w/ dep on resolv
	 "-lasn1 -lcrypto -lcom_err $LIB_roken -ldb" dnl heimdal-BSD w/ roken w/ dep on db
	; do
  LIBS="$saved_LIBS"
  if test "X$1" != "X"; then
    LIBS="$LIBS -L$1"
  fi
  LIBS="$LIBS -lkrb5 $a"
  AC_TRY_LINK([],
  [krb5_init_context();],
  [ac_cv_found_krb5_lib=yes
   ac_cv_found_krb5=yes
   ac_cv_krb5_extralib=$a
   break],
  ac_cv_found_krb5_lib=no)
done
LIBS=$saved_LIBS
])

AC_DEFUN(AC_KRB5_LIB_WHERE, [
   for i in $1; do
      AC_MSG_CHECKING(for kerberos5 libraries in $i)
      AC_KRB5_LIB_WHERE1($i)
      if test "$ac_cv_found_krb5_lib" != "no" ; then
        ac_cv_krb5_where_lib=$i
	AC_MSG_RESULT(found)
	break
      else
	AC_MSG_RESULT(not found)
      fi
    done
])



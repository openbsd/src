dnl
dnl $Id: check-kafs.m4,v 1.1 2000/09/11 14:40:45 art Exp $
dnl
dnl check for libkafs/krbafs
dnl

AC_DEFUN(AC_CHECK_KAFS,[

AC_ARG_WITH(krbafs,
[  --with-krbafs=dir       use libkrbafs (from cmu, extracted from kth-krb) in dir],

[if test "$with_krbafs" = "yes"; then
  AC_MSG_ERROR([You have to give the path to krbafs lib])
elif test "$with_krbafs" = "no"; then
  ac_cv_funclib_k_hasafs=no
else
  KAFS_LIBS_FLAGS="-L${with_krbafs}/lib"
fi])

AC_CACHE_CHECK([for libkafs/libkrbafs],
[ac_cv_funclib_k_hasafs],[

saved_LIBS="$LIBS"

for a in "$KRB4_LIB_FLAGS" \
	 "$KRB5_LIB_FLAGS" \
	 "$KRB5_LIB_FLAGS $KRB4_LIB_FLAGS" ; do
  for b in "kafs" "krbafs"; do
    LIBS="$saved_LIBS ${KAFS_LIBS_FLAGS} $a -l$b"
    AC_TRY_LINK([],
    [k_hasafs();],
    [ac_cv_funclib_k_hasafs=yes
    ac_cv_libkafs_flags="$KAFS_LIBS_FLAGS $a -l$b"
    break 2],
    [ac_cv_funclib_k_hasafs=no])
  done
done

LIBS="$saved_LIBS"])

if test "X$ac_cv_funclib_k_hasafs" != "Xno"; then
   KAFS_LIBS="$ac_cv_libkafs_flags"
fi

AC_SUBST(KAFS_LIBS)dnl

])dnl

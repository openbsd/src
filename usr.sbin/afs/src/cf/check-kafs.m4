dnl
dnl $KTH: check-kafs.m4,v 1.7.2.1 2001/04/16 23:07:48 lha Exp $
dnl
dnl check for libkafs/krbafs
dnl

dnl check_kafs_fluff(LIB_DIR,LIB_LIBS)
define(check_kafs_fluff,[
for b in "kafs" "krbafs"; do
  LIBS="$saved_LIBS ${KAFS_LIBS_FLAGS} $1 -l$b $2"
  AC_TRY_LINK([],
  [k_hasafs()],
  [ac_cv_funclib_k_hasafs=yes
  ac_cv_libkafs_flags="$KAFS_LIBS_FLAGS $1 -l$b $2"
  break 2],
  [ac_cv_funclib_k_hasafs=no])
done])

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

for a in "foo" ; do
check_kafs_fluff([],[])
check_kafs_fluff([${KRB4_LIB_DIR}],[${KRB4_LIB_LIBS}])
check_kafs_fluff([${KRB5_LIB_DIR}],[${KRB5_LIB_LIBS}])
check_kafs_fluff([${KRB4_LIB_DIR} ${KRB5_LIB_DIR}],
[${KRB5_LIB_LIBS} ${KRB4_LIB_LIBS}])
done

undefine([check_kafs_fluff])

LIBS="$saved_LIBS"])

if test "X$ac_cv_funclib_k_hasafs" != "Xno"; then
   KAFS_LIBS="$ac_cv_libkafs_flags"
fi

AC_SUBST(KAFS_LIBS)dnl

])dnl

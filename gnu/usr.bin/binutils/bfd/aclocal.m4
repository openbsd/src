dnl
dnl This ugly hack is needed because the Cygnus configure script won't
dnl tell us what CC is going to be, and "cc" isn't always right.  (The
dnl top-level Makefile will always override anything we choose here, so
dnl the usual gcc/cc selection is useless.)
dnl
dnl It knows where it is in the tree; don't try using it elsewhere.
dnl
undefine([AC_PROG_CC])dnl
AC_DEFUN(AC_PROG_CC,
[AC_BEFORE([$0], [AC_PROG_CPP])dnl
dnl
dnl The ugly bit...
dnl
AC_MSG_CHECKING([for CC])
dnl Don't bother with cache.
test -z "$CC" && test -r ../Makefile && CC=`egrep '^CC *=' ../Makefile | tail -1 | sed 's/^CC *= *//'`
test -z "$CC" && CC=cc
AC_MSG_RESULT(setting CC to $CC)
AC_SUBST(CC)
dnl
dnl Find out if we are using GNU C, under whatever name.
dnl The semicolon is to pacify NeXT's syntax-checking cpp.
cat > conftest.c <<EOF
#ifdef __GNUC__
  yes;
#endif
EOF
if ${CC-cc} -E conftest.c 2>&AC_FD_CC | egrep yes >/dev/null 2>&1; then
  GCC=yes
  if test "${CFLAGS+set}" != set; then
    echo 'void f(){}' > conftest.c
    if test -z "`${CC-cc} -g -c conftest.c 2>&1`"; then
      CFLAGS="-g -O"
    else
      CFLAGS="-O"
    fi
  fi
else
  GCC=
  test "${CFLAGS+set}" = set || CFLAGS="-g"
fi
rm -f conftest*
])dnl
dnl
AC_DEFUN(BFD_BINARY_FOPEN,
[case "${host}" in
changequote(,)dnl
i[345]86-*-msdos* | i[345]86-*-go32* | i[345]86-*-win32)
changequote([,])dnl
  AC_DEFINE(USE_BINARY_FOPEN) ;;
esac])dnl
dnl
AC_DEFUN(BFD_CC_FOR_BUILD,
[# Put a plausible default for CC_FOR_BUILD in Makefile.
AC_REQUIRE([AC_C_CROSS])dnl
if test -z "$CC_FOR_BUILD"; then
  if test "x$cross_compiling" = "xno"; then
    CC_FOR_BUILD='$(CC)'
  else
    CC_FOR_BUILD=gcc
  fi
fi
AC_SUBST(CC_FOR_BUILD)])dnl

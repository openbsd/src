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
test -z "$CC" && CC=`egrep '^CC *=' ../Makefile | tail -1 | sed 's/^CC *= *//'`
test -z "$CC" && CC=cc
AC_MSG_RESULT(setting CC to $CC)
AC_SUBST(CC)
dnl
dnl
# Find out if we are using GNU C, under whatever name.
cat > conftest.c <<EOF
#ifdef __GNUC__
  yes
#endif
EOF
${CC-cc} -E conftest.c > conftest.out 2>&1
if egrep yes conftest.out >/dev/null 2>&1; then
  GCC=yes
else
  GCC=
fi
rm -f conftest*
])dnl
dnl
dnl GAS_CHECK_DECL_NEEDED(name, typedefname, typedef, headers)
AC_DEFUN(GAS_CHECK_DECL_NEEDED,[
AC_MSG_CHECKING(whether declaration is required for $1)
AC_CACHE_VAL(gas_cv_decl_needed_$1,
AC_TRY_LINK([$4],
[
typedef $3;
$2 x;
x = ($2) $1;
], gas_cv_decl_needed_$1=no, gas_cv_decl_needed_$1=yes))dnl
AC_MSG_RESULT($gas_cv_decl_needed_$1)
test $gas_cv_decl_needed_$1 = no || {
 ifelse(index($1,[$]),-1,
    [AC_DEFINE([NEED_DECLARATION_]translit($1, [a-z], [A-Z]))],
    [gas_decl_name_upcase=`echo $1 | tr '[a-z]' '[A-Z]'`
     AC_DEFINE_UNQUOTED(NEED_DECLARATION_$gas_decl_name_upcase)])
}
])dnl
dnl
dnl Some non-ANSI preprocessors botch requoting inside strings.  That's bad
dnl enough, but on some of those systems, the assert macro relies on requoting
dnl working properly!
dnl GAS_WORKING_ASSERT
AC_DEFUN(GAS_WORKING_ASSERT,
[AC_MSG_CHECKING([for working assert macro])
AC_CACHE_VAL(gas_cv_assert_ok,
AC_TRY_LINK([#include <assert.h>
#include <stdio.h>], [
/* check for requoting problems */
static int a, b, c, d;
static char *s;
assert (!strcmp(s, "foo bar baz quux"));
/* check for newline handling */
assert (a == b
        || c == d);
], gas_cv_assert_ok=yes, gas_cv_assert_ok=no))dnl
AC_MSG_RESULT($gas_cv_assert_ok)
test $gas_cv_assert_ok = yes || AC_DEFINE(BROKEN_ASSERT)
])dnl
dnl
dnl Since many Bourne shell implementations lack subroutines, use this
dnl hack to simplify the code in configure.in.
dnl GAS_UNIQ(listvar)
AC_DEFUN(GAS_UNIQ,
[_gas_uniq_list="[$]$1"
_gas_uniq_newlist=""
dnl Protect against empty input list.
for _gas_uniq_i in _gas_uniq_dummy [$]_gas_uniq_list ; do
  case [$]_gas_uniq_i in
  _gas_uniq_dummy) ;;
  *) case " [$]_gas_uniq_newlist " in
       *" [$]_gas_uniq_i "*) ;;
       *) _gas_uniq_newlist="[$]_gas_uniq_newlist [$]_gas_uniq_i" ;;
     esac ;;
  esac
done
$1=[$]_gas_uniq_newlist
])dnl

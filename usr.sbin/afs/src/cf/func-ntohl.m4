dnl
dnl $KTH: func-ntohl.m4,v 1.1 1999/05/15 22:45:26 assar Exp $
dnl
dnl test for how to do ntohl
dnl

AC_DEFUN(AC_FUNC_NTOHL, [
AC_REQUIRE([AC_CANONICAL_TARGET])
AC_MSG_CHECKING(for efficient ntohl)
AC_CACHE_VAL(ac_cv_func_ntohl, [
case "$target_cpu" in
changequote(, )dnl
i[3-9]86) AC_TRY_RUN(
changequote([, ])dnl
[
#if defined(__GNUC__) && defined(i386)
unsigned long foo(unsigned long x)
{
  asm("bswap %0" : "=r" (x) : "0" (x));
  return x;
}
#endif

int main(void)
{
  return foo(0x12345678) != 0x78563412;
}
],
ac_cv_func_ntohl="bswap",
ac_cv_func_ntohl="ntohl",
ac_cv_func_ntohl="ntohl") ;;
alpha) ac_cv_func_ntohl="bswap32" ;;
*) ac_cv_func_ntohl="ntohl" ;;
esac
])
AC_MSG_RESULT($ac_cv_func_ntohl)
AC_DEFINE_UNQUOTED(EFF_NTOHL, $ac_cv_func_ntohl, [how should ntohl be done?])
])

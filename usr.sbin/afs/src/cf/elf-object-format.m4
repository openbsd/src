dnl
dnl $Id: elf-object-format.m4,v 1.1 2000/09/11 14:40:47 art Exp $
dnl
dnl test for ELF

AC_DEFUN(AC_ELF_OBJECT_FORMAT,[
AC_CACHE_CHECK([for ELF object format], ac_cv_sys_elf_object_format,[
ac_cv_sys_elf_object_format=no
echo 'int foo;' > conftest.$ac_ext
if AC_TRY_EVAL(ac_compile); then
	case `file conftest.o 2> /dev/null` in
	*ELF*)	ac_cv_sys_elf_object_format=yes ;;
	esac
fi
rm -f conftest*])])

dnl $KTH: krb-version.m4,v 1.3 2000/06/03 20:31:44 lha Exp $
dnl
dnl
dnl output a C header-file with some version strings
dnl
AC_DEFUN(AC_KRB_VERSION,[
dnl AC_OUTPUT_COMMANDS([
cat > include/${PACKAGE}-newversion.h.in <<FOOBAR
char *${PACKAGE}_long_version = "@(#)\$Version: $PACKAGE-$VERSION by @USER@ on @HOST@ ($host) @DATE@ \$";
char *${PACKAGE}_version = "$PACKAGE-$VERSION";
FOOBAR

if test -f include/${PACKAGE}-version.h && cmp -s include/${PACKAGE}-newversion.h.in include/${PACKAGE}-version.h.in; then
	echo "include/${PACKAGE}-version.h is unchanged"
	rm -f include/${PACKAGE}-newversion.h.in
else
 	echo "creating include/${PACKAGE}-version.h"
 	User=${USER-${LOGNAME}}
 	Host=`(hostname || uname -n) 2>/dev/null | sed 1q`
 	Date=`date`
	mv -f include/${PACKAGE}-newversion.h.in include/${PACKAGE}-version.h.in
	sed -e "s/@USER@/$User/" -e "s/@HOST@/$Host/" -e "s/@DATE@/$Date/" include/${PACKAGE}-version.h.in > include/${PACKAGE}-version.h
fi
dnl ],host=$host PACKAGE=$PACKAGE VERSION=$VERSION)
])

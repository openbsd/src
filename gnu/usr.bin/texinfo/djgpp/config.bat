@echo off

echo Configuring GNU Texinfo for DJGPP v2.x...

Rem The small_env tests protect against fixed and too small size
Rem of the environment in stock DOS shell.

Rem Find out if NLS is wanted or not, if dependency-tracking is
Rem wanted or not, if cache is wanted or not, and where the sources are.
set ARGS=
set NLS=disabled
if not "%NLS%" == "disabled" goto small_env
set CACHE=enabled
if not "%CACHE%" == "enabled" goto small_env
set DEPTRAK=disabled
if not "%DEPTRAK%" == "disabled" goto small_env
set XSRC=.
if not "%XSRC%" == "." goto small_env

Rem Loop over all arguments.
Rem Special arguments are: NLS, XSRC CACHE and DEPS.
Rem All other arguments are stored into ARGS.
:arg_loop
set SPECARG=0
if not "%SPECARG%" == "0" goto small_env
if not "%1" == "NLS" if not "%1" == "nls" goto cache_opt
if "%1" == "nls" set NLS=enabled
if "%1" == "NLS" set NLS=enabled
if not "%NLS%" == "enabled" goto small_env
set SPECARG=1
if not "%SPECARG%" == "1" goto small_env
shift
:cache_opt
set SPECARG=0
if not "%SPECARG%" == "0" goto small_env
if "%1" == "no-cache" goto cache_off
if "%1" == "no-CACHE" goto cache_off
if not "%1" == "NO-CACHE" goto dependency_opt
:cache_off
if "%1" == "no-cache" set CACHE=disabled
if "%1" == "no-CACHE" set CACHE=disabled
if "%1" == "NO-CACHE" set CACHE=disabled
if not "%CACHE%" == "disabled" goto small_env
set SPECARG=1
if not "%SPECARG%" == "1" goto small_env
shift
:dependency_opt
set SPECARG=0
if not "%SPECARG%" == "0" goto small_env
if "%1" == "dep" goto dep_off
if not "%1" == "DEP" goto src_dir_opt
:dep_off
if "%1" == "dep" set DEPTRAK=enabled
if "%1" == "DEP" set DEPTRAK=enabled
if not "%DEPTRAK%" == "enabled" goto small_env
set SPECARG=1
if not "%SPECARG%" == "1" goto small_env
shift
:src_dir_opt
set SPECARG=0
if not "%SPECARG%" == "0" goto small_env
echo %1 | grep -q "/"
if errorlevel 1 goto collect_arg
set XSRC=%1
if not "%XSRC%" == "%1" goto small_env
set SPECARG=1
if not "%SPECARG%" == "1" goto small_env
:collect_arg
if "%SPECARG%" == "0" set _ARGS=%ARGS% %1
if "%SPECARG%" == "0" if not "%_ARGS%" == "%ARGS% %1" goto small_env
echo %_ARGS% | grep -q "[^ ]"
if not errorlevel 0 set ARGS=%_ARGS%
set _ARGS=
shift
if not "%1" == "" goto arg_loop
set SPECARG=

Rem Create a response file for the configure script.
echo --srcdir=%XSRC% > arguments
if "%CACHE%" == "enabled"    echo --config-cache >>arguments
if "%DEPTRAK%" == "enabled"  echo --enable-dependency-tracking >>arguments
if "%DEPTRAK%" == "disabled" echo --disable-dependency-tracking >>arguments
if not "%ARGS%" == ""        echo %ARGS% >>arguments
set ARGS=
set CACHE=
set DEPTRAK=

if "%XSRC%" == "." goto in_place

:not_in_place
redir -e /dev/null update %XSRC%/configure.orig ./configure
test -f ./configure
if errorlevel 1 update %XSRC%/configure ./configure

:in_place
Rem Update configuration files
echo Updating configuration scripts...
test -f ./configure.orig
if errorlevel 1 update configure configure.orig
sed -f %XSRC%/djgpp/config.sed configure.orig > configure
if errorlevel 1 goto sed_error

Rem Make sure they have a config.site file
set CONFIG_SITE=%XSRC%/djgpp/config.site
if not "%CONFIG_SITE%" == "%XSRC%/djgpp/config.site" goto small_env

Rem Make sure crucial file names are not munged by unpacking
test -f %XSRC%/po/Makefile.in.in
if not errorlevel 1 mv -f %XSRC%/po/Makefile.in.in %XSRC%/po/Makefile.in-in
test -f %XSRC%/po/Makefile.am.in
if not errorlevel 1 mv -f %XSRC%/po/Makefile.am.in %XSRC%/po/Makefile.am-in

Rem This is required because DOS/Windows are case-insensitive
Rem to file names, and "make install" will do nothing if Make
Rem finds a file called `install'.
if exist INSTALL ren INSTALL INSTALL.txt

Rem Set HOME to a sane default so configure stops complaining.
if not "%HOME%" == "" goto host_name
set HOME=%XSRC%/djgpp
if not "%HOME%" == "%XSRC%/djgpp" goto small_env
echo No HOME found in the environment, using default value

:host_name
Rem Set HOSTNAME so it shows in config.status
if not "%HOSTNAME%" == "" goto hostdone
if "%windir%" == "" goto msdos
set OS=MS-Windows
if not "%OS%" == "MS-Windows" goto small_env
goto haveos
:msdos
set OS=MS-DOS
if not "%OS%" == "MS-DOS" goto small_env
:haveos
if not "%USERNAME%" == "" goto haveuname
if not "%USER%" == "" goto haveuser
echo No USERNAME and no USER found in the environment, using default values
set HOSTNAME=Unknown PC
if not "%HOSTNAME%" == "Unknown PC" goto small_env
goto userdone
:haveuser
set HOSTNAME=%USER%'s PC
if not "%HOSTNAME%" == "%USER%'s PC" goto small_env
goto userdone
:haveuname
set HOSTNAME=%USERNAME%'s PC
if not "%HOSTNAME%" == "%USERNAME%'s PC" goto small_env
:userdone
set _HOSTNAME=%HOSTNAME%, %OS%
if not "%_HOSTNAME%" == "%HOSTNAME%, %OS%" goto small_env
set HOSTNAME=%_HOSTNAME%
:hostdone
set _HOSTNAME=
set OS=

Rem install-sh is required by the configure script but clashes with the
Rem various Makefile install-foo targets, so we MUST have it before the
Rem script runs and rename it afterwards
test -f %XSRC%/install-sh
if not errorlevel 1 goto no_ren0
test -f %XSRC%/install-sh.sh
if not errorlevel 1 mv -f %XSRC%/install-sh.sh %XSRC%/install-sh
:no_ren0

if "%NLS%" == "disabled" goto without_NLS

:with_NLS
Rem Check for the needed libraries and binaries.
test -x /dev/env/DJDIR/bin/msgfmt.exe
if not errorlevel 0 goto missing_NLS_tools
test -x /dev/env/DJDIR/bin/xgettext.exe
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/include/libcharset.h
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/lib/libcharset.a
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/include/iconv.h
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/lib/libiconv.a
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/include/libintl.h
if not errorlevel 0 goto missing_NLS_tools
test -f /dev/env/DJDIR/lib/libintl.a
if not errorlevel 0 goto missing_NLS_tools

Rem Recreate the files in the %XSRC%/po subdir with our ported tools.
redir -e /dev/null rm %XSRC%/po/*.gmo
redir -e /dev/null rm %XSRC%/po/diffutil*.pot
redir -e /dev/null rm %XSRC%/po/cat-id-tbl.c
redir -e /dev/null rm %XSRC%/po/stamp-cat-id

Rem Update the arguments file for the configure script.
Rem We prefer without-included-gettext because libintl.a from gettext package
Rem is the only one that is guaranteed to have been ported to DJGPP.
echo --enable-nls --without-included-gettext >> arguments
goto configure_package

:missing_NLS_tools
echo Needed libs/tools for NLS not found.  Configuring without NLS.
:without_NLS
Rem Update the arguments file for the configure script.
echo --disable-nls >> arguments

:configure_package
echo Running the ./configure script...
sh ./configure @arguments
if errorlevel 1 goto cfg_error
rm arguments

Rem Remove files created by the gl_FUNC_MKSTEMP test.
rm co*.tmp
echo Done.
goto End

:sed_error
echo ./configure script editing failed!
goto End

:cfg_error
echo ./configure script exited abnormally!
goto End

:small_env
echo Your environment size is too small.  Enlarge it and run me again.
echo Configuration NOT done!

:End
test -f %XSRC%/install-sh.sh
if not errorlevel 1 goto no_ren1
test -f %XSRC%/install-sh
if not errorlevel 1 mv -f %XSRC%/install-sh %XSRC%/install-sh.sh
:no_ren1
if "%HOME%" == "%XSRC%/djgpp" set HOME=
set ARGS=
set CONFIG_SITE=
set HOSTNAME=
set NLS=
set CACHE=
set DEPTRAK=
set XSRC=

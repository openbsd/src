@echo off
set CONFIG=
set PATH_SEPARATOR=;
set PATH_EXPAND=y
sh -c 'if test $PATH_SEPARATOR = ";"; then exit 1; fi'
if ERRORLEVEL 1 goto path_sep_ok
echo Error:
echo Make sure the environment variable PATH_SEPARATOR=; while building perl!
echo Please check your DJGPP.ENV!
goto end

:path_sep_ok
sh -c 'if test $PATH_EXPAND = "Y" -o $PATH_EXPAND = "y"; then exit 1; fi'
if ERRORLEVEL 1 goto path_exp_ok
echo Error:
echo Make sure the environment variable PATH_EXPAND=Y while building perl!
echo Please check your DJGPP.ENV!
goto end

:path_exp_ok
sh -c '$SHELL -c "exit 128"'
if ERRORLEVEL 128 goto shell_ok

echo Error:
echo The SHELL environment variable must be set to the full path of your sh.exe!
goto end

:shell_ok
sh -c 'if test ! -d /tmp; then mkdir /tmp; fi'
cp djgpp.[hc] config.over ..
cd ..
echo Running sed...
sh djgpp/djgppsed.sh

echo Running Configure...
sh Configure %1 %2 %3 %4 %5 %6 %7 %8 %9
:end

@echo off

::- This script must be executed on the PC with an ActiveSync
::- connection. If it does not work, create the entries with
::- a remote registry editor or get a registry editor for your
::- devices.
::-
::- You need my cereg.exe program.

::- My paths...
set perlexe=\speicherkarte2\bin\perl.exe
set perllib=\speicherkarte2\usr\lib\perl5

::- PERL5LIB
cereg -k "HKLM\Environment" -n "PERL5LIB" -v "%perllib%"

::- For ShellExecute
cereg -k "HKCR\.pl" -n "" -v "perlfile"
cereg -k "HKCR\perlfile" -n "" -v "Perl Script"
cereg -k "HKCR\perlfile\DefaultIcon" -n "" -v "%perlexe%,-1"

::- You might need to fix the quotes if your paths contain spaces!
cereg -k "HKCR\perlfile\Shell\open\command" -n "" -v "%perlexe% %%1"

cereg -k "HKLM\Environment" -n "ROWS" -v "10"
cereg -k "HKLM\Environment" -n "COLS" -v "75"
cereg -k "HKLM\Environment" -n "PATH" -v "/Speicherkarte2/bin"
cereg -k "HKLM\Environment" -n "UNIXROOTDIR" -v "/Speicherkarte2"

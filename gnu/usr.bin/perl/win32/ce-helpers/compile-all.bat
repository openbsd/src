@echo off
rem
rem Normally you do not need to run this file.
rem Instead you should edit and execute compile.bat .
rem
rem This file assumes that you have a set of appropriate
rem bat-files that prepare environment variables for build process
rem and execute commands passed as arguments
rem

call wcearm-300 compile.bat "MACHINE=wince-arm-hpc-wce300"
call wcearm-300 compile.bat "MACHINE=wince-arm-hpc-wce300" zipdist
..\miniperl makedist.pl --clean-exts

call wcearm-211 compile.bat "MACHINE=wince-arm-hpc-wce211"
call wcearm-211 compile.bat "MACHINE=wince-arm-hpc-wce211"  zipdist
..\miniperl makedist.pl --clean-exts

call wcesh3-211 compile.bat "MACHINE=wince-sh3-hpc-wce211"
call wcesh3-211 compile.bat "MACHINE=wince-sh3-hpc-wce211"  zipdist
..\miniperl makedist.pl --clean-exts

call wcemips-211 compile.bat "MACHINE=wince-mips-hpc-wce211"
call wcemips-211 compile.bat "MACHINE=wince-mips-hpc-wce211"  zipdist
..\miniperl makedist.pl --clean-exts

rem TODO call wcesh3-200 compile.bat "MACHINE=wince-sh3-hpc-wce200"
rem TODO call wcesh3-200 compile.bat "MACHINE=wince-sh3-hpc-wce200"  zipdist
rem TODO ..\miniperl makedist.pl --clean-exts

rem TODO call compile.bat "MACHINE=wince-mips-hpc-wce200"
rem TODO call compile.bat "MACHINE=wince-mips-hpc-wce200"  zipdist
rem TODO ..\miniperl makedist.pl --clean-exts

call WCEARM-p300 compile.bat "MACHINE=wince-arm-pocket-wce300"
call WCEARM-p300 compile.bat "MACHINE=wince-arm-pocket-wce300"  zipdist
..\miniperl makedist.pl --clean-exts

call WCEMIPS-300 compile.bat "MACHINE=wince-mips-pocket-wce300"
call WCEMIPS-300 compile.bat "MACHINE=wince-mips-pocket-wce300"  zipdist
..\miniperl makedist.pl --clean-exts

call WCESH3-300 compile.bat "MACHINE=wince-sh3-pocket-wce300"
call WCESH3-300 compile.bat "MACHINE=wince-sh3-pocket-wce300"  zipdist
..\miniperl makedist.pl --clean-exts

call WCEx86-300 compile.bat "MACHINE=wince-x86em-pocket-wce300"
call WCEx86-300 compile.bat "MACHINE=wince-x86em-pocket-wce300"  zipdist
..\miniperl makedist.pl --clean-exts

call WCEMIPS-palm211 compile.bat "MACHINE=wince-mips-palm-wce211"
call WCEMIPS-palm211 compile.bat "MACHINE=wince-mips-palm-wce211"  zipdist
..\miniperl makedist.pl --clean-exts

call WCESH3-palm211 compile.bat "MACHINE=wince-sh3-palm-wce211"
call WCESH3-palm211 compile.bat "MACHINE=wince-sh3-palm-wce211"  zipdist
..\miniperl makedist.pl --clean-exts

call WCEx86-palm211 compile.bat "MACHINE=wince-x86em-palm-wce211"
call WCEx86-palm211 compile.bat "MACHINE=wince-x86em-palm-wce211"  zipdist
..\miniperl makedist.pl --clean-exts


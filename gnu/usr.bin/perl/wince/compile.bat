@echo off
rem
rem edit ARG-xxx variable to reflect your system and run
rem   compile.bat [target] [additional parameters for nmake]
rem

set ARG-1=PV=
set ARG-2=INST_VER=
set ARG-3=INSTALL_ROOT=\Storage Card\perl-tests\perl@16376
set ARG-4=WCEROOT=%SDKROOT%
set ARG-5=CEPATH=%WCEROOT%
set ARG-6=CELIBDLLDIR=d:\personal\pocketPC\celib-palm-3.0
set ARG-7=CECONSOLEDIR=d:\personal\pocketPC\w32console

rem Only for WIN2000
set ARG-8=YES=/y

set ARG-9=CFG=RELEASE
set ARG-10=MACHINE=wince-mips-pocket-wce300
set ARG-11=PERLCEDIR=$(MAKEDIR)
set ARG-12=MSVCDIR=D:\MSVStudio\VC98
set ARG-13=CECOPY=$(HPERL) -I$(PERLCEDIR)\lib $(PERLCEDIR)\comp.pl --copy
set ARG-14=USE_PERLIO=undef

nmake -f Makefile.ce "%ARG-1%" "%ARG-2%" "%ARG-3%" "%ARG-4%" "%ARG-5%" "%ARG-6%" "%ARG-7%" "%ARG-8%" "%ARG-9%" "%ARG-10%" "%ARG-11%" "%ARG-12%" "%ARG-13%" "%ARG-14%" %1 %2 %3 %4 %5 %6 %7 %8 %9

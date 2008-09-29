@echo off
rem
rem edit ARG-xxx variable to reflect your system and run
rem   compile.bat [target] [additional parameters for nmake]
rem

set ARG-1=PV=
set ARG-2=INST_VER=
set ARG-3=INSTALL_ROOT=\Storage Card\perl58m
set ARG-4=WCEROOT=%SDKROOT%
set ARG-5=CEPATH=%WCEROOT%
set ARG-6=CELIBDLLDIR=d:\personal\pocketPC\celib-palm-3.0
set ARG-7=CECONSOLEDIR=d:\personal\pocketPC\w32console

rem Only for WIN2000
set ARG-8=YES=/y

set ARG-9=CFG=RELEASE

rem
rem  uncomment one of following lines that matches your configuration

rem set ARG-10=MACHINE=wince-mips-pocket-wce300
rem set ARG-10=MACHINE=wince-arm-hpc-wce300
rem set ARG-10=MACHINE=wince-arm-hpc-wce211
rem set ARG-10=MACHINE=wince-sh3-hpc-wce211
rem set ARG-10=MACHINE=wince-mips-hpc-wce211
rem set ARG-10=MACHINE=wince-sh3-hpc-wce200
rem set ARG-10=MACHINE=wince-mips-hpc-wce200
rem set ARG-10=MACHINE=wince-arm-pocket-wce300
rem set ARG-10=MACHINE=wince-mips-pocket-wce300
rem set ARG-10=MACHINE=wince-sh3-pocket-wce300
rem set ARG-10=MACHINE=wince-x86em-pocket-wce300
rem set ARG-10=MACHINE=wince-mips-palm-wce211
rem set ARG-10=MACHINE=wince-sh3-palm-wce211
rem set ARG-10=MACHINE=wince-x86em-palm-wce211

set ARG-11=PERLCEDIR=$(MAKEDIR)
set ARG-12=MSVCDIR=D:\MSVStudio\VC98
set ARG-13=CECOPY=$(HPERL) -I$(PERLCEDIR)\lib $(PERLCEDIR)\comp.pl --copy

nmake -f Makefile.ce "%ARG-1%" "%ARG-2%" "%ARG-3%" "%ARG-4%" "%ARG-5%" "%ARG-6%" "%ARG-7%" "%ARG-8%" "%ARG-9%" "%ARG-10%" "%ARG-11%" "%ARG-12%" "%ARG-13%" %1 %2 %3 %4 %5 %6 %7 %8 %9

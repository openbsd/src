echo off
rem
rem  Copyright (C) 2000, 2001  Internet Software Consortium.
rem 
rem  Permission to use, copy, modify, and distribute this software for any
rem  purpose with or without fee is hereby granted, provided that the above
rem  copyright notice and this permission notice appear in all copies.
rem 
rem  THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
rem  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
rem  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
rem  INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
rem  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
rem  FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
rem  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
rem  WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

rem BuildSetup.bat
rem This script sets up the files necessary ready to build BIND 9.
rem This requires perl to be installed on the system.

rem Set up the configuration file
cd ..
copy config.h.win32 config.h
cd win32utils

rem Generate the version information
perl makeversion.pl

rem Generate header files for lib/dns

call dnsheadergen.bat

echo Ensure that the OpenSSL sources are at the same level in
echo the directory tree and is named openssl-0.9.6e or libdns
echo will not build. 

rem Make sure that the Build directories are there.

if NOT Exist ..\Build mkdir ..\Build
if NOT Exist ..\Build\Release mkdir ..\Build\Release

echo Copying the ARM and the Installation Notes.
 
copy ..\COPYRIGHT ..\Build\Release
copy readme1st.txt ..\Build\Release
copy ..\doc\arm\*.html ..\Build\Release
copy ..\CHANGES ..\Build\Release
copy ..\FAQ ..\Build\Release

echo Copying the OpenSSL DLL.

copy ..\..\openssl-0.9.6e\out32dll\libeay32.dll ..\Build\Release\


rem Done

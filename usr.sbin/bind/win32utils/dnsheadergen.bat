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

cd ..\lib\dns
cd win32
nmake /nologo /f gen.mak CFG="gen - Win32 Release"  NO_EXTERNAL_DEPS="1"
cd ..
gen -s . -t > include/dns/enumtype.h
gen -s . -c > include/dns/enumclass.h
gen -s . -i -P ./rdata/rdatastructpre.h -S ./rdata/rdatastructsuf.h > include/dns/rdatastruct.h
gen -s . > code.h
cd ..\..\win32utils

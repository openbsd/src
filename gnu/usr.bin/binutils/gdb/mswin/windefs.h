
/*
 * The purpose of this file is to force the order of header files 
 * to resolve namespace conflicts between gdb and windows.
 * Include this file instead of including <windows.h> directly.
 * 
 * These files contain defs for booleans and cause conflicts:
 *    gdb\mswin\win-e7kpc.h:#define boolean forgive_me
 *    gdb\mswin\prebuilt\bfd.h:#define boolean _boolean
 * 
 *    gdb\mswin\prebuilt\bfd.h:typedef enum bfd_boolean {false, true} boolean;
 *    gdb\mswin\prebuilt\bfd.h:typedef enum bfd_boolean {bfd_fffalse, bfd_tttrue} boolean;
 *    bfd\bfd-in.h:typedef enum bfd_boolean {false, true} boolean;
 *    bfd\bfd-in.h:typedef enum bfd_boolean {bfd_fffalse, bfd_tttrue} boolean;
 *    bfd\bfd-in2.h:typedef enum bfd_boolean {false, true} boolean;
 *    bfd\bfd-in2.h:typedef enum bfd_boolean {bfd_fffalse, bfd_tttrue} boolean;
 *    c:\msvc22\include\RPCNDR.H:typedef unsigned char boolean;
 * 
 * defs.h includes bfd.h which defines boolean...
 * windows.h includes rplndr.h which defines boolean...
 * We ensure that windows.h is the last to get included by 
 * including defs.h first, preventing defs.h from getting 
 * included again afterwards.
 * 
 * Files which need windows.h:
 *    gdb\win32-nat.c
 *    gdb\mswin\win-e7kpc.h
 *    gdb\mswin\initfake.c
 *    gdb\mswin\iostuff.c
 *    gdb\mswin\serdll16.c
 *    gdb\mswin\serdll32.c
 *    gdb\mswin\stubs.c
 * 
 */

#ifndef _WINDEFS_H_
#define _WINDEFS_H_

#ifdef __WINDOWS_H__
#error DO NOT INCLUDE WINDOWS FILES OUTSIDE OF THIS HEADER!
#endif

#undef PTR
#undef PARAMS
#define PTR void *
#define PARAMS(x) x

#if defined(_MSC_VER) && (_MSC_VER > 800)
/* 16 bit msvc (_MSC_VER=800) doesn't need these */
#include "../defs.h"
#else
#include <stdio.h>
#define GDB_FILE FILE
#define ATTR_FORMAT(x,y,z)
#endif
#undef min                      /* These come from stdlib.h */
#undef max

#include "gdbcmd.h"
#include "serial.h"
#include <errno.h>
/* #define boolean forgive_me */
#define boolean msvc_boolean
#include <windows.h>
#undef boolean

#if 0
/* An alternate method; having bfd prevent window's from overriding it's def */

/* MAKE SURE THIS FOLLOWS ANY WINDOWS INCLUDES!! */
#ifdef _MSC_VER
/* msvc's rpcndr.h typedefs boolean as char, so we 
 * define it as something else so we don't conflict 
 */
#define boolean wingdb_boolean

#ifdef __RPCNDR_H__
/* #error boolean already defined !! */
#else
/* FIXME!!!  kludge to make sure boolean doesn't get redefined! */
#define __RPCNDR_H__
#endif

#endif /* _MSC_VER */

#endif /* 0 */

#endif /* _WINDEFS_H_ */


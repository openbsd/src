
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	CLIBstr.h
 * DESCRIPTION	:	Forces the use of clib string.h calls over static watcom calls
 *                  for C/C++ applications that statically link watcom libraries.
 *
 *                  This file must be included each time that string.h is included.
 *                  In the case of the Perl project, just include string.h and
 *                  the make should take care of the rest.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#ifndef _CLIBSTR_H_
#define _CLIBSTR_H_


#ifdef DEFINE_GPF
#define _GPFINIT =0
#define _GPFEXT
#else
#define _GPFINIT
#define _GPFEXT extern
#endif

#ifdef __cplusplus
extern "C"
{
#endif

_GPFEXT void* gpf_memchr _GPFINIT;
_GPFEXT void* gpf_memcmp _GPFINIT;
_GPFEXT void* gpf_memcpy _GPFINIT;
_GPFEXT void* gpf_memmove _GPFINIT;
_GPFEXT void* gpf_memset _GPFINIT;
_GPFEXT void* gpf_strchr _GPFINIT;
_GPFEXT void* gpf_strcmp _GPFINIT;
_GPFEXT void* gpf_strcoll _GPFINIT;
_GPFEXT void* gpf_strcspn _GPFINIT;
_GPFEXT void* gpf_strerror _GPFINIT;
_GPFEXT void* gpf_strtok_r _GPFINIT;
_GPFEXT void* gpf_strpbrk _GPFINIT;
_GPFEXT void* gpf_strrchr _GPFINIT;
_GPFEXT void* gpf_strspn _GPFINIT;
_GPFEXT void* gpf_strstr _GPFINIT;
_GPFEXT void* gpf_strtok _GPFINIT;
_GPFEXT void* gpf_strxfrm _GPFINIT;
_GPFEXT void* gpf_memicmp _GPFINIT;
_GPFEXT void* gpf_strcmpi _GPFINIT;
_GPFEXT void* gpf_stricmp _GPFINIT;
_GPFEXT void* gpf_strrev _GPFINIT;
_GPFEXT void* gpf_strupr _GPFINIT;

_GPFEXT void* gpf_strcpy _GPFINIT;
_GPFEXT void* gpf_strcat _GPFINIT;
_GPFEXT void* gpf_strlen _GPFINIT;
_GPFEXT void* gpf_strncpy _GPFINIT;
_GPFEXT void* gpf_strncat _GPFINIT;
_GPFEXT void* gpf_strncmp _GPFINIT;
_GPFEXT void* gpf_strnicmp _GPFINIT;
_GPFEXT void* gpf_strdup _GPFINIT;
_GPFEXT void* gpf_strlist _GPFINIT;
_GPFEXT void* gpf_strlwr _GPFINIT;
_GPFEXT void* gpf_strnset _GPFINIT;
_GPFEXT void* gpf_strset _GPFINIT;

#ifdef __cplusplus
}
#endif

#pragma aux memchr = "call gpf_memchr";
#pragma aux memcmp = "call gpf_memcmp";
#pragma aux memcpy = "call gpf_memcpy";
#pragma aux memmove = "call gpf_memmove";
#pragma aux memset = "call gpf_memset";
#pragma aux strchr = "call gpf_strchr";
#pragma aux strcmp = "call gpf_strcmp";
#pragma aux strcoll = "call gpf_strcoll";
#pragma aux strcspn = "call gpf_strcspn";
#pragma aux strerror = "call gpf_strerror";
#pragma aux strtok_r = "call gpf_strtok_r";
#pragma aux strpbrk = "call gpf_strpbrk";
#pragma aux strrchr = "call gpf_strrchr";
#pragma aux strspn = "call gpf_strspn";
#pragma aux strstr = "call gpf_strstr";
#pragma aux strtok = "call gpf_strtok";
#pragma aux strxfrm = "call gpf_strxfrm";
#pragma aux memicmp = "call gpf_memicmp";
#pragma aux strcmpi = "call gpf_strcmpi";
#pragma aux stricmp = "call gpf_stricmp";
#pragma aux strrev = "call gpf_strrev";
#pragma aux strupr = "call gpf_strupr";

#pragma aux strcpy = "call gpf_strcpy";
#pragma aux strcat = "call gpf_strcat";
#pragma aux strlen = "call gpf_strlen";
#pragma aux strncpy = "call gpf_strncpy";
#pragma aux strncat = "call gpf_strncat";
#pragma aux strncmp = "call gpf_strncmp";
#pragma aux strnicmp = "call gpf_strnicmp";
#pragma aux strdup = "call gpf_strdup";
#pragma aux strlist = "call gpf_strlist";
#pragma aux strlwr = "call gpf_strlwr";
#pragma aux strnset = "call gpf_strnset";
#pragma aux strset = "call gpf_strset";


#endif	// _CLIBSTR_H_


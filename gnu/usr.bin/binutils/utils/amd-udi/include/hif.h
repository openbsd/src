/* @(#)hif.h	5.19 93/10/26 11:33:44, Srini, AMD */
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 * This header file defines the error codes, service numbers for the HIF
 * kernel.
 *****************************************************************************
 */

#ifndef	_HIF_H_INCLUDED_
#define	_HIF_H_INCLUDED_

#define MAX_ENV               256
#define MAX_FILENAME          256

#define MAX_OPEN_FILES         20

#define HIF_SUCCESS    0x80000000

/*
** HIF services
*/

#define HIF_exit            1
#define HIF_open           17
#define HIF_close          18
#define HIF_read           19
#define HIF_write          20
#define HIF_lseek          21
#define HIF_remove         22
#define HIF_rename         23
#define HIF_ioctl          24
#define HIF_iowait         25
#define HIF_iostat         26
#define HIF_tmpnam         33
#define HIF_time           49
#define HIF_getenv         65
#define HIF_gettz          66


/*
** HIF Error codes
*/

#define HIF_EPERM               1
#define HIF_ENOENT              2
#define HIF_ESRCH               3
#define HIF_EINTR               4
#define HIF_EIO                 5
#define HIF_ENXIO               6
#define HIF_E2BIG               7
#define HIF_ENOEXEC             8
#define HIF_EBADF               9
#define HIF_ECHILD             10
#define HIF_EAGAIN             11
#define HIF_ENOMEM             12
#define HIF_EACCESS            13
#define HIF_EFAULT             14
#define HIF_ENOTBLK            15
#define HIF_EBUSY              16
#define HIF_EEXIST             17
#define HIF_EXDEV              18
#define HIF_ENODEV             19
#define HIF_ENOTDIR            20
#define HIF_EISDIR             21
#define HIF_EINVAL             22
#define HIF_ENFILE             23
#define HIF_EMFILE             24
#define HIF_ENOTTY             25
#define HIF_ETXTBSY            26
#define HIF_EFBIG              27
#define HIF_ENOSPC             28
#define HIF_ESPIPE             29
#define HIF_EROFS              30
#define HIF_EMLINK             31
#define HIF_EPIPE              32
#define HIF_EDOM               33
#define HIF_ERANGE             34
#define HIF_EWOULDBLOCK        35
#define HIF_EINPROGRESS        36
#define HIF_EALREADY           37
#define HIF_ENOTSOCK           38
#define HIF_EDESTADDRREQ       39
#define HIF_EMSGSIZE           40
#define HIF_EPROTOTYPE         41
#define HIF_ENOPROTOOPT        42
#define HIF_EPROTONOSUPPORT    43
#define HIF_ESOCKTNOSUPPORT    44
#define HIF_EOPNOTSUPP         45
#define HIF_EPFNOSUPPORT       46
#define HIF_EAFNOSUPPORT       47
#define HIF_EADDRINUSE         48
#define HIF_EADDRNOTAVAIL      49
#define HIF_ENETDOWN           50
#define HIF_ENETUNREACH        51
#define HIF_ENETRESET          52
#define HIF_ECONNABORTED       53
#define HIF_ECONNRESET         54
#define HIF_ENOBUFS            55
#define HIF_EISCONN            56
#define HIF_ENOTCONN           57
#define HIF_ESHUTDOWN          58
#define HIF_ETOOMANYREFS       59
#define HIF_ETIMEDOUT          60
#define HIF_ECONNREFUSED       61
#define HIF_ELOOP              62
#define HIF_ENAMETOOLONG       63
#define HIF_EHOSTDOWN          64
#define HIF_EHOSTUNREACH       65
#define HIF_ENOTEMPTY          66
#define HIF_EPROCLIM           67
#define HIF_EUSERS             68
#define HIF_EDQUOT             69
#define HIF_EVDBAD             70
#define HIF_EHIFNOTAVAIL     1001
#define HIF_EHIFUNDEF        1002

/*
** Open service mode parameters
*/

#define HIF_RDONLY     0x0000
#define HIF_WRONLY     0x0001
#define HIF_RDWR       0x0002
#define HIF_APPEND     0x0008
#define HIF_NDELAY     0x0010
#define HIF_CREAT      0x0200
#define HIF_TRUNC      0x0400
#define HIF_EXCL       0x0800
#define HIF_FORM       0x4000

/*
** iostat definitions
*/

#define ISATTY         0x0001
#define RDREADY        0x0002


/*
** Fix differences between BSD UNIX and MS-DOS in <fcntl.h>
*/

#if MSDOS

#define O_NDELAY       0x0000

#else

#define O_BINARY       0x0000
#define O_TEXT         0x0000

#endif


#endif /* _HIF_H_INCLUDED_ */

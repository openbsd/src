/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 *
 * logging.h - methods for logging warnings, errors and trace info
 */

#ifndef angel_logging_h
#define angel_logging_h

#include <stdarg.h>

/*
 * __rt_warning
 * ------------
 * Provides a standard method of generating run-time system
 * warnings. The actual action taken by this code can be board or
 * target application specific, e.g. internal logging, debug message,
 * etc.
 */
extern void __rt_warning(char *format, ...);

/*---------------------------------------------------------------------------*/

/*
 * __rt_error
 * ----------
 * Raise an internal Angel error. The parameters are passed directly
 * to "__rt_warning" for display, and the code then raises a debugger
 * event and stops the target processing.
 */
extern void __rt_error(char *format, ...);

/*
 * Some macros for debugging and warning messages
 */

typedef enum WarnLevel {
    WL_TRACE,
    WL_WARN,
    WL_ERROR
} WarnLevel;

void va_warn(WarnLevel level, char *format, va_list args);

#ifdef _WINGDI_
/* stupidity in MSVC <wingdi.h> (in <windows.h> in <winsock.h>) */
# undef ERROR
#endif

#ifndef ERROR
# define ERROR_FORMAT "Error \"%s\" in %s at line %d\n"
# define ERROR(e) __rt_error(ERROR_FORMAT, (e), __FILE__, __LINE__)
#endif

#ifndef ASSERT
# ifdef ASSERTIONS_ENABLED
#   define ASSERT(x, y) ((x) ? (void)(0) : ERROR((y)))
# else
#   define ASSERT(x, y) ((void)(0))
# endif
#endif

#ifndef WARN
# ifdef ASSERTIONS_ENABLED
#   define WARN_FORMAT "Warning \"%s\" in %s at line %d\n"
#   define WARN(w) __rt_warning(WARN_FORMAT, (w), __FILE__, __LINE__)
# else
#   define WARN(w) ((void)(0))
# endif
#endif


#ifdef NO_INFO_MESSAGES
# define __rt_info (void)
# ifndef INFO
#   define INFO(w)
# endif
#else
# define __rt_info __rt_warning
# ifndef INFO
#  ifdef DEBUG
#   define INFO(w) __rt_warning("%s\n", (w))
#  else
#   define INFO(w) ((void)(0))
#  endif
# endif
#endif


#if defined(DEBUG) && !defined(NO_IDLE_CHITCHAT)
# ifndef DO_TRACE
#   define DO_TRACE (1)
# endif
#endif

#ifdef DO_TRACE
extern void __rt_trace(char *format, ...);
#endif

#ifndef TRACE
# ifdef DO_TRACE
#   define TRACE(w) __rt_trace("%s ", (w))
# else
#   define TRACE(w) ((void)(0))
# endif
#endif

#endif /* ndef angel_logging_h */

/* EOF logging.h */

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
 * logging.c - methods for logging warnings, errors and trace info
 *
 */

#include <stdarg.h>     /* ANSI varargs support */

#ifdef TARGET
# include "angel.h"
# include "devconf.h"
#else
# include "host.h"
#endif

#include "logging.h"    /* Header file for this source code */

#ifndef UNUSED
# define UNUSED(x) ((x)=(x))
#endif

/*
 * __rt_warning
 * ------------
 * This routine is provided as a standard method of generating
 * run-time system warnings. The actual action taken by this code can
 * be board or target application specific, e.g. internal logging,
 * debug message, etc.
 */

#ifdef DEBUG

# ifdef DEBUG_METHOD

#  define  STRINGIFY2(x) #x
#  define  STRINGIFY(x)  STRINGIFY2(x)
#  define  DEBUG_METHOD_HEADER        STRINGIFY(DEBUG_METHOD##.h)

#  include DEBUG_METHOD_HEADER

#  define  METHOD_EXPAND_2(m, p, c) m##p(c)
#  define  METHOD_EXPAND(m, p, c)   METHOD_EXPAND_2(m, p, c)

#  define  CHAROUT(c)    METHOD_EXPAND(DEBUG_METHOD, _PutChar,  (c))
#  define  PRE_DEBUG(l)  METHOD_EXPAND(DEBUG_METHOD, _PreWarn,  (l))
#  define  POST_DEBUG(n) METHOD_EXPAND(DEBUG_METHOD, _PostWarn, (n))

# else
#  error Must define DEBUG_METHOD
# endif

#endif /* def DEBUG */

/*
 * the guts of __rt_warning
 */

#pragma no_check_stack
#ifdef DEBUG

static const char hextab[] = "0123456789ABCDEF";

/*
 * If debugging, then we break va_warn into sub-functions which
 * allow us to get an easy breakpoint on the formatted string
 */
static int va_warn0(char *format, va_list args)
{
    int len = 0;

    while ((format != NULL) && (*format != '\0'))
    {
        if (*format == '%')
        {
            char fch = *(++format); /* get format character (skipping '%') */
            int ival; /* holder for integer arguments */
            char *string; /* holder for string arguments */
            int width = 0; /* No field width by default */
            int padzero = FALSE; /* By default we pad with spaces */

            /*
             * Check if the format has a width specified. NOTE: We do
             * not use the "isdigit" function here, since it will
             * require run-time support. The current ARM Ltd header
             * defines "isdigit" as a macro, that uses a fixed
             * character description table.
             */
            if ((fch >= '0') && (fch <= '9'))
            {
                if (fch == '0')
                {
                    /* Leading zeroes padding */
                    padzero = TRUE;
                    fch = *(++format);
                }

                while ((fch >= '0') && (fch <= '9'))
                {
                    width = ((width * 10) + (fch - '0'));
                    fch = *(++format);
                }
            }

            if (fch == 'l')
                /* skip 'l' in "%lx", etc. */
                fch = *(++format);

            switch (fch)
            {
              case 'c':
                  /* char */
                  ival = va_arg(args, int);
                  CHAROUT((char)ival);
                  len++;
                  break;

              case 'x':
              case 'X':
              {
                  /* hexadecimal */
                  unsigned int uval = va_arg(args, unsigned int);
                  int loop;

                  UNUSED(uval);

                  if ((width == 0) || (width > 8))
                      width = 8;

                  for(loop = (width * 4); (loop != 0); loop -= 4)
                  {
                      CHAROUT(hextab[(uval >> (loop - 4)) & 0xF]);
                      len++;
                  }
              }

              break;

              case 'd':
                  /* decimal */
                  ival = va_arg(args, int);

                  if (ival < 0)
                  {
                      ival = -ival;
                      CHAROUT('-');
                      len++;
                  }

                  if (ival == 0)
                  {
                      CHAROUT('0');
                      len++;
                  }
                  else
                  {
                      /*
                       * The simplest method of displaying numbers is
                       * to provide a small recursive routine, that
                       * nests until the most-significant digit is
                       * reached, and then falls back out displaying
                       * individual digits. However, we want to avoid
                       * using recursive code within the lo-level
                       * parts of Angel (to minimise the stack
                       * usage). The following number conversion is a
                       * non-recursive solution.
                       */
                      char buffer[16]; /* stack space used to hold number */
                      int count = 0; /* pointer into buffer */

                      /*
                       * Place the conversion into the buffer in
                       * reverse order:
                       */
                      while (ival != 0)
                      {
                          buffer[count++] = ('0' + ((unsigned int)ival % 10));
                          ival = ((unsigned int)ival / 10);
                      }

                      /*
                       * Check if we are placing the data in a
                       * fixed width field:
                       */
                      if (width != 0)
                      {
                          width -= count;

                          for (; (width != 0); width--)
                          {
                              CHAROUT(padzero ? '0': ' ');
                              len++;
                          }
                      }

                      /* then display the buffer in reverse order */
                      for (; (count != 0); count--)
                      {
                          CHAROUT(buffer[count - 1]);
                          len++;
                      }
                  }

                  break;

              case 's':
                  /* string */
                  string = va_arg(args, char *);

                  /* we only need this test once */
                  if (string != NULL)
                      /* whilst we check this for every character */
                      while (*string)
                      {
                          CHAROUT(*string);
                          len++;
                          string++;

                          /*
                           * NOTE: We do not use "*string++" as the macro
                           * parameter, since we do not know how many times
                           *the parameter may be expanded within the macro.
                           */
                      }

                  break;

              case '\0':
                  /*
                   * string terminated by '%' character, bodge things
                   * to prepare for default "format++" below
                   */
                  format--;

                  break;

              default:
                  /* just display the character */
                  CHAROUT(*format);
                  len++;

                  break;
            }

            format++; /* step over format character */
        }
        else
        {
            CHAROUT(*format);
            len++;
            format++;
        }
    }
    return len;
}

/*
 * this routine is simply here as a good breakpoint for dumping msg -
 * can be used by DEBUG_METHOD macros or functions, if required.
 */
# ifdef DEBUG_NEED_VA_WARN1
static void va_warn1(int len, char *msg)
{
    UNUSED(len); UNUSED(msg);
}
# endif

void va_warn(WarnLevel level, char *format, va_list args)
{
    int len;

    if ( PRE_DEBUG( level ) )
    {
        len = va_warn0(format, args);
        POST_DEBUG( len );
    }
}

#else /* ndef DEBUG */

void va_warn(WarnLevel level, char *format, va_list args)
{
    UNUSED(level); UNUSED(format); UNUSED(args);
}

#endif /* ... else ndef(DEBUG) ... */
#pragma check_stack

#pragma no_check_stack
void __rt_warning(char *format, ...)
{
    va_list args;

    /*
     * For a multi-threaded system we should provide a lock at this point
     * to ensure that the warning messages are sequenced properly.
     */

    va_start(args, format);
    va_warn(WL_WARN, format, args);
    va_end(args);

    return;
}
#pragma check_stack

#ifdef TARGET

#pragma no_check_stack
void __rt_uninterruptable_loop( void ); /* in suppasm.s */

void __rt_error(char *format, ...)
{
    va_list args;

    va_start(args, format);

    /* Display warning message */
    va_warn(WL_ERROR, format, args);

    __rt_uninterruptable_loop();

    va_end(args);
    return;
}
#pragma check_stack

#endif /* def TARGET */

#ifdef DO_TRACE

static bool trace_on = FALSE; /* must be set true in debugger if req'd */

#pragma no_check_stack
void __rt_trace(char *format, ...)
{
    va_list args;

    /*
     * For a multi-threaded system we should provide a lock at this point
     * to ensure that the warning messages are sequenced properly.
     */

    if (trace_on)
    {
        va_start(args, format);
        va_warn(WL_TRACE, format, args);
        va_end(args);
    }

    return;
}
#pragma check_stack

#endif /* def DO_TRACE */


/* EOF logging.c */

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
 */

#ifdef __hpux
#  define _POSIX_SOURCE 1
#endif

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#ifdef __hpux
#  define _TERMIOS_INCLUDED
#  include <sys/termio.h>
#  undef _TERMIOS_INCLUDED
#else
#  include <termios.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>

#if defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (bsdi)
#undef BSD
#include <sys/ioctl.h>
#endif

#ifdef sun
# include <sys/ioccom.h>
# ifdef __svr4__
#  include <sys/bpp_io.h>
# else
#  include <sbusdev/bpp_io.h>
# endif
#endif

#ifdef BSD
# ifdef sun
#  include <sys/ttydev.h>
# endif
# ifdef __alpha 
#  include <sys/ioctl.h>
# else
#  include <sys/filio.h>
# endif
#endif

#ifdef __hpux
#  define _INCLUDE_HPUX_SOURCE
#  include <sys/ioctl.h>
#  undef _INCLUDE_HPUX_SOURCE
#endif

#include "host.h"
#include "unixcomm.h"

#define PP_TIMEOUT      1              /* seconds */

#ifdef sun
#define SERIAL_PREFIX "/dev/tty"
#define SERPORT1   "/dev/ttya"
#define SERPORT2   "/dev/ttyb"
#define PARPORT1   "/dev/bpp0"
#define PARPORT2   "/dev/bpp1"
#endif

#ifdef __hpux
#define SERIAL_PREFIX "/dev/tty"
#define SERPORT1   "/dev/tty00"
#define SERPORT2   "/dev/tty01"
#define PARPORT1   "/dev/ptr_parallel"
#define PARPORT2   "/dev/ptr_parallel"
#endif

#ifdef __linux__
#define SERIAL_PREFIX "/dev/ttyS"
#define SERPORT1   "/dev/ttyS0"
#define SERPORT2   "/dev/ttyS1"
#define PARPORT1   "/dev/par0"
#define PARPORT2   "/dev/par1"
#endif

#if defined(_WIN32) || defined (__CYGWIN__) 
#define SERIAL_PREFIX "com"
#define SERPORT1   "com1"
#define SERPORT2   "com2"
#define PARPORT1   "lpt1"
#define PARPORT2   "lpt2"
#endif

#if !defined (SERIAL_PREFIX)
#define SERIAL_PREFIX "/dev/cuaa"
#define SERPORT1   "/dev/cuaa0"
#define SERPORT2   "/dev/cuaa1"
#define PARPORT1   "/dev/lpt0"
#define PARPORT2   "/dev/lpt1"
#endif




/*
 * Parallel port output pins, used for signalling to target
 */

#ifdef sun
struct bpp_pins bp;
#endif

static int serpfd = -1;
static int parpfd = -1;

extern const char *Unix_MatchValidSerialDevice(const char *name)
{
  int i=0;
  char *sername=NULL;

  /* Accept no name as the default serial port */
  if (name == NULL) {
    return SERPORT1;
  }

  /* Look for the simple cases - 1,2,s,S,/dev/... first, and
   * afterwards look for S=... clauses, which need parsing properly.
   */

  /* Accept /dev/tty* where * is limited */
  if (strlen(name) == strlen(SERPORT1)
      && strncmp(name, SERIAL_PREFIX, strlen (SERIAL_PREFIX)) == 0)
      {
        return name;
      }

  /* Accept "1" or "2" or "S" - S is equivalent to "1" */
  if (strcmp(name, "1") == 0 ||
      strcmp(name, "S") == 0 || strcmp(name, "s") == 0) {
    return SERPORT1;
  }
  if (strcmp(name, "2") == 0) return SERPORT2;

  /* It wasn't one of the simple cases, so now we have to parse it
   * properly
   */

  do {
    switch (name[i]) {
      case ',':
        /* Skip over commas */
        i++;
        break;
      
      default:
        return 0;
        /* Unexpected character => error - not matched */

      case 0:
        /* End of string means return whatever we have matched */
        return sername;

      case 's':
      case 'S':
      case 'h':
      case 'H': {
        char ch = tolower(name[i]);
        int j, continue_from, len;
        
        /* If the next character is a comma or a NULL then this is
         * a request for the default Serial port
         */
        if (name[++i] == 0 || name[i] == ',') {
          if (ch=='s') 
              sername=SERPORT1;
          break;
        }

        /* Next character must be an = */
        if (name[i] != '=') return 0;
        /* Search for the end of the port spec. (ends in NULL or ,) */
        for (j= ++i; name[j] != 0 && name[j] != ','; j++)
          ; /* Do nothing */
        /* Notice whether this is the last thing to parse or not
         * and also calaculate the length of the string
         */
        if (name[j] == '0') continue_from = -1;
        else continue_from = j;
        len=(j-i);

        /* And now try to match the serial / parallel port */
        switch (ch) {
          case 's': {
            /* Match serial port */
            if (len==1) {
              if (name[i]=='1') 
                  sername=SERPORT1;
              else if (name[i]=='2') 
                  sername=SERPORT2;
            } else if (len==strlen(SERPORT1)) {
              if (strncmp(name+i,SERPORT1,strlen(SERPORT1)) == 0)
                sername=SERPORT1;
              else if (strncmp(name+i,SERPORT2,strlen(SERPORT2)) == 0)
                sername=SERPORT2;
            }

            break;
          }

          case 'h': 
            /* We don't actually deal with the H case here, we just
             * match it and allow it through.
             */
            break;
        }

        if (continue_from == -1) return sername;
        i = continue_from;
        break;
      }
    }
  } while (1);

  return 0;
}


extern int Unix_IsSerialInUse(void)
{
    if (serpfd >= 0)
        return -1;

    return 0;
}

extern int Unix_OpenSerial(const char *name)
{
#if defined(BSD) || defined(__CYGWIN__)
    serpfd = open(name, O_RDWR);
#else
    serpfd = open(name, O_RDWR | O_NONBLOCK);
#endif

    if (serpfd < 0) {
        perror("open");
        return -1;
    }
#ifdef TIOCEXCL
    if (ioctl(serpfd, TIOCEXCL) < 0) {
	close(serpfd);
        perror("ioctl: TIOCEXCL");
        return -1;
    }
#endif

    return 0;
}

extern void Unix_CloseSerial(void)
{
    if (serpfd >= 0)
    {
        (void)close(serpfd);
        serpfd = -1;
    }
}

extern int Unix_ReadSerial(unsigned char *buf, int n, bool block)
{
    fd_set fdset;
    struct timeval tv;
    int err;

    FD_ZERO(&fdset);
    FD_SET(serpfd, &fdset);

    tv.tv_sec = 0;
    tv.tv_usec = (block ? 10000 : 0);

    err = select(serpfd + 1, &fdset, NULL, NULL, &tv);

    if (err < 0 && errno != EINTR)
    {
#ifdef DEBUG
        perror("select");
#endif
        panic("select failure");
        return -1;
    }
    else if (err > 0 && FD_ISSET(serpfd, &fdset))
      {
	int s;

	s = read(serpfd, buf, n);
	if (s < 0)
	  perror("read:");
	return s;
      }
    else /* err == 0 || FD_CLR(serpfd, &fdset) */
    {
        errno = ERRNO_FOR_BLOCKED_IO;
        return -1;
    }
}

extern int Unix_WriteSerial(unsigned char *buf, int n)
{
    return write(serpfd, buf, n);
}

extern void Unix_ResetSerial(void)
{
    struct termios terminfo;

    tcgetattr(serpfd, &terminfo);
    terminfo.c_lflag &= ~(ICANON | ISIG | ECHO | IEXTEN);
    terminfo.c_iflag &= ~(IGNCR | INPCK | ISTRIP | ICRNL | BRKINT);
    terminfo.c_iflag |= (IXON | IXOFF | IGNBRK);
    terminfo.c_cflag = (terminfo.c_cflag & ~CSIZE) | CS8 | CREAD;
    terminfo.c_cflag &= ~PARENB;
    terminfo.c_cc[VMIN] = 1;
    terminfo.c_cc[VTIME] = 0;
    terminfo.c_oflag &= ~OPOST;
    tcsetattr(serpfd, TCSAFLUSH, &terminfo);
}

extern void Unix_SetSerialBaudRate(int baudrate)
{
    struct termios terminfo;

    tcgetattr(serpfd, &terminfo);
    cfsetospeed(&terminfo, baudrate);
    cfsetispeed(&terminfo, baudrate);
    tcsetattr(serpfd, TCSAFLUSH, &terminfo);
}

extern void Unix_ioctlNonBlocking(void)
{
#if defined(BSD)
    int nonblockingIO = 1;
    (void)ioctl(serpfd, FIONBIO, &nonblockingIO);

    if (parpfd != -1)
        (void)ioctl(parpfd, FIONBIO, &nonblockingIO);
#endif
}

extern void Unix_IsValidParallelDevice(
  const char *portstring, char **sername, char **parname)
{
  int i=0;
  *sername=NULL;
  *parname=NULL;

  /* Do not recognise a NULL portstring */
  if (portstring==NULL) return;

  do {
    switch (portstring[i]) {
      case ',':
        /* Skip over commas */
        i++;
        break;
      
      default:
      case 0:
        /* End of string or bad characcter means we have finished */
        return;

      case 's':
      case 'S':
      case 'p':
      case 'P':
      case 'h':
      case 'H': {
        char ch = tolower(portstring[i]);
        int j, continue_from, len;
        
        /* If the next character is a comma or a NULL then this is
         * a request for the default Serial or Parallel port
         */
        if (portstring[++i] == 0 || portstring[i] == ',') {
          if (ch=='s') *sername=SERPORT1;
          else if (ch=='p') *parname=PARPORT1;
          break;
        }

        /* Next character must be an = */
        if (portstring[i] != '=') return;
        /* Search for the end of the port spec. (ends in NULL or ,) */
        for (j= ++i; portstring[j] != 0 && portstring[j] != ','; j++)
          ; /* Do nothing */
        /* Notice whether this is the last thing to parse or not
         * and also calaculate the length of the string
         */
        if (portstring[j] == '0') continue_from = -1;
        else continue_from = j;
        len=(j-i);

        /* And now try to match the serial / parallel port */
        switch (ch) {
          case 's': {
            /* Match serial port */
            if (len==1) {
              if (portstring[i]=='1') *sername=SERPORT1;
              else if (portstring[i]=='2') *sername=SERPORT2;
            } else if (len==strlen(SERPORT1)) {
              if (strncmp(portstring+i,SERPORT1,strlen(SERPORT1)) == 0)
                *sername=SERPORT1;
              else if (strncmp(portstring+i,SERPORT2,strlen(SERPORT2)) == 0)
                *sername=SERPORT2;
            }
            break;
          }

          case 'p': {
            /* Match parallel port */
            if (len==1) {
              if (portstring[i]=='1') *parname=PARPORT1;
              else if (portstring[i]=='2') *parname=PARPORT2;
            } else if (len==strlen(PARPORT1)) {
              if (strncmp(portstring+i,PARPORT1,strlen(PARPORT1)) == 0)
                *parname=PARPORT1;
              else if (strncmp(portstring+i,PARPORT2,strlen(PARPORT2)) == 0)
                *parname=PARPORT2;
            }
            break;
          }

          case 'h': 
            /* We don't actually deal with the H case here, we just
             * match it and allow it through.
             */
            break;
        }

        if (continue_from == -1) return;
        i = continue_from;
        break;
      }
    }
  } while (1);
  return;  /* Will never get here */
}

extern int Unix_IsParallelInUse(void)
{
    if (parpfd >= 0)
        return -1;

    return 0;
}

extern int Unix_OpenParallel(const char *name)
{
#if defined(BSD)
    parpfd = open(name, O_RDWR);
#else
    parpfd = open(name, O_RDWR | O_NONBLOCK);
#endif

    if (parpfd < 0)
    {
        char errbuf[256];

        sprintf(errbuf, "open %s", name);
        perror(errbuf);

        return -1;
    }

    return 0;
}

extern void Unix_CloseParallel(void)
{
    if (parpfd >= 0)
    {
        (void)close(parpfd);
        parpfd = -1;
    }
}


extern unsigned int Unix_WriteParallel(unsigned char *buf, int n)
{
    int ngone;

    if ((ngone = write(parpfd, buf, n)) < 0)
    {
        /*
         * we ignore errors (except for debug purposes)
         */
#ifdef DEBUG
        char errbuf[256];

        sprintf(errbuf, "send_packet: write");
        perror(errbuf);
#endif
        ngone = 0;
    }

    /* finished */
    return (unsigned int)ngone;
}


#ifdef sun
extern void Unix_ResetParallel(void)
{
    struct bpp_transfer_parms tp;

#ifdef DEBUG
    printf("serpar_reset\n");
#endif

    /*
     * we need to set the parallel port up for BUSY handshaking,
     * and select the timeout
     */
    if (ioctl(parpfd, BPPIOC_GETPARMS, &tp) < 0)
    {
#ifdef DEBUG
        perror("ioctl(BPPIOCGETPARMS)");
#endif
        panic("serpar_reset: cannot get BPP parameters");
    }

    tp.write_handshake = BPP_BUSY_HS;
    tp.write_timeout = PP_TIMEOUT;

    if (ioctl(parpfd, BPPIOC_SETPARMS, &tp) < 0)
    {
#ifdef DEBUG
        perror("ioctl(BPPIOC_SETPARMS)");
#endif
        panic("serpar_reset: cannot set BPP parameters");
    }
}

#else

/* Parallel not supported on HP */

extern void Unix_ResetParallel(void)
{
}

#endif


static char _[] = "@(#)hif.c	5.19 93/10/26 11:33:19, Srini, AMD.";
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
 *      Engineers: Srini Subramanian.
 *****************************************************************************
 * This module implements the part of the HIF kernel implemented using the
 * host computer's operating system. This is invoked when a HIF_CALL message
 * is received by MONTIP. This module uses the Debugger/Monitor on the 29K
 * target for certain HIF services.
 * The results are sent back to the 29K Target using a HIF_CALL_RTN message.
 * If the Debugger is invoked, a GO message is sent first to switch the
 * context from the Debugger to the HIF kernel, and then the HIF_CALL_RTN
 * message is sent. 
 *****************************************************************************
 */

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <memory.h>
#include "types.h"
#include "udiproc.h"

#include "hif.h"

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif 
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef	MSDOS
#include <io.h>
#include <sys\types.h>
#include <sys\timeb.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <sys/timeb.h>
#endif

#include "mtip.h"	/* access to version numbers, etc. */
#include "messages.h"

INT32   UDI_write_string PARAMS((INT32, ADDR32, INT32, char *));
INT32   UDI_read_string PARAMS((INT32, ADDR32, INT32, char *));

INT32   hif_exit PARAMS((UINT32 lr2));
INT32   hif_open PARAMS((UINT32 lr2, UINT32 lr3, UINT32 lr4, UINT32 *gr96));
INT32   hif_close PARAMS((UINT32 lr2, UINT32 *gr96));
INT32   hif_read PARAMS((UINT32 lr2, UINT32 lr3, UINT32 lr4, UINT32 *gr96));
INT32   hif_write PARAMS((UINT32 lr2, UINT32 lr3, UINT32 lr4, UINT32 *gr96));
INT32   hif_lseek PARAMS((UINT32 lr2, UINT32 lr3, UINT32 lr4, UINT32 *gr96));
INT32   hif_remove PARAMS((UINT32 lr2, UINT32 *gr96));
INT32   hif_rename PARAMS((UINT32 lr2, UINT32 lr3, UINT32 *gr96));
INT32   hif_ioctl PARAMS((UINT32 lr2, UINT32 lr3));
INT32   hif_iowait PARAMS((UINT32 lr2, UINT32 lr3));
INT32   hif_iostat PARAMS((UINT32 lr2, UINT32 *gr96));
INT32   hif_tmpnam PARAMS((UINT32 lr2, UINT32 *gr96));
INT32   hif_time PARAMS((UINT32 *gr96));
INT32   hif_getenv PARAMS((UINT32 lr2, UINT32  lr3, UINT32 *gr96));
INT32   hif_gettz PARAMS((UINT32 *gr96, UINT32 *gr97));


#ifdef	MSDOS
/* Stub out functions not available in MS-DOS */
int   ioctl(int x, int y);
int   ioctl(x, y) int x; int y; {return(0);}
void  kbd_raw()     {return;}
void  kbd_restore() {return;}
#endif

/*
** Globals
*/

#define	TMP_BUF_SIZE	1024

static	char tmp_buffer[TMP_BUF_SIZE];

static	int	com_error;


/*
** This function is the HIF service handler on the host. It receives
** the HIF system call message as its only parameter.  When
** complete, GR96, GR97 and GR121 should be set to appropriate
** values.  
**
*/

INT32
service_HIF(service_number, lr2, lr3, lr4, gr96, gr97, gr121)
UINT32		service_number;
UINT32		lr2, lr3, lr4;
UINT32		*gr96, *gr97, *gr121;
   {
   INT32  result;

   com_error  = 0;	/* reset */
   *gr121 = (UINT32) 0x80000000; /* success unless... */

   switch ((int) service_number) {
      case HIF_exit:   result = hif_exit(lr2);
                       break;
      case HIF_open:   result = hif_open(lr2, lr3, lr4, gr96);
                       break;
      case HIF_close:  result = hif_close(lr2, gr96);
                       break;
      case HIF_read:   result = hif_read(lr2, lr3, lr4, gr96);
                       break;
      case HIF_write:  result = hif_write(lr2, lr3, lr4, gr96);
                       break;
      case HIF_remove: result = hif_remove(lr2, gr96);
                       break;
      case HIF_lseek:  result = hif_lseek(lr2, lr3, lr4, gr96);
                       break;
      case HIF_rename: result = hif_rename(lr2, lr3, gr96);
                       break;
      case HIF_ioctl:  result = hif_ioctl(lr2, lr3);
                       break;
      case HIF_iowait: result = hif_iowait(lr2, lr3);
                       break;
      case HIF_iostat: result = hif_iostat(lr2, gr96);
                       break;
      case HIF_tmpnam: result = hif_tmpnam(lr2, gr96);
                       break;
      case HIF_time:   result = hif_time(gr96);
                       break;
      case HIF_getenv: result = hif_getenv(lr2, lr3, gr96);
                       break;
      case HIF_gettz:  result = hif_gettz(gr96, gr97);
                       break;
      default:         *gr121 = (UINT32) HIF_EHIFUNDEF;
			return (SUCCESS);
      }

   /* Set gr121 if HIF error received */
   if (result != (INT32) 0)
      *gr121 = (UINT32) result;

   if (com_error) 
     return ((INT32) -1);	/* FAILURE */
   return(SUCCESS);

   }  /* end service_HIF() */




/*
** HIF Services
*/


/*
** Service 1 - exit
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    1 (0x01)    Service number
**             lr2      exitcode    User-supplied exit code
**
** Returns:    gr96     undefined   Service does not return
**             gr121    undefined   Service does not return
**
*/

/* ARGSUSED */
INT32
hif_exit(lr2)
UINT32	lr2;
   {
   return (0);
   }  /* end hif_exit() */


/*
** Service 17 - open
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    17 (0x11)   Service number
**             lr2      pathname    Pointer to a filename
**             lr3      mode        See HIF Specification
**             lr4      pflag       See HIF Specification
**
** Returns:    gr96     fileno      Success >= 0 (file descriptor)
**                                  Failure <0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
*/

INT32
hif_open(lr2, lr3, lr4, gr96)
UINT32	lr2, lr3, lr4;
UINT32	*gr96;
   {
   INT32      result;
   int      hif_mode;
   int      host_mode;
   int      hif_pflag;
   int      fd;
   ADDR32   pathptr;


   pathptr = (ADDR32) lr2;
   hif_pflag = (int) lr4;
   hif_mode = (int) lr3;

   /*
   ** Note:  The pflag is the same in BSD and MS-DOS.
   ** Unfortunately, the O_* flags for HIF, UNIX and MS-DOS
   ** are not the same.  So we have to do the folowing:
   */

   host_mode = 0;
   if ((hif_mode & HIF_RDONLY) != 0)
      host_mode = host_mode | O_RDONLY;
   if ((hif_mode & HIF_WRONLY) != 0)
      host_mode = host_mode | O_WRONLY;
   if ((hif_mode & HIF_RDWR) != 0)
      host_mode = host_mode | O_RDWR;
   if ((hif_mode & HIF_APPEND) != 0)
      host_mode = host_mode | O_APPEND;
   if ((hif_mode & HIF_NDELAY) != 0)
      host_mode = host_mode | O_NDELAY;
   if ((hif_mode & HIF_CREAT) != 0)
      host_mode = host_mode | O_CREAT;
   if ((hif_mode & HIF_TRUNC) != 0)
      host_mode = host_mode | O_TRUNC;
   if ((hif_mode & HIF_EXCL) != 0)
      host_mode = host_mode | O_EXCL;
   if ((hif_mode & HIF_FORM) != 0)
      host_mode = host_mode | O_TEXT;
      else
         host_mode = host_mode | O_BINARY;


   result = UDI_read_string((INT32) UDI29KDRAMSpace, 
                        pathptr,
                        (INT32) MAX_FILENAME,
                        tmp_buffer);

   if (result != (INT32) 0) {
      *gr96 = (UINT32) -1;
      return((INT32) result);
   }

   fd = open(tmp_buffer, host_mode, hif_pflag);

   *gr96 = (UINT32) fd;

   if (fd < 0)  /* open failed on host */
      return((INT32) errno);

   return (0);
   }  /* end hif_open() */



/*
** Service 18 - close
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    18 (0x12)   Service number
**             lr2      fileno      File descriptor
**
** Returns:    gr96     retval      Success = 0
**                                  Failure < 0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

INT32
hif_close(lr2, gr96)
UINT32	lr2;
UINT32	*gr96;
   {
   int  file_no;
   int  retval;

   file_no = (int) lr2;


   /* Don't close stdin, stdout or stderr */
   if (file_no == 0 ||
       file_no == 1 ||
       file_no == 2)
      retval = 0;  /* Pretend we closed it */
      else
         retval = close(file_no);

   *gr96 = (UINT32) retval;

   if (retval < 0) 
      return((INT32) errno);

   return (0);
   }  /* end hif_close() */




/*
** Service 19 - read
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    19 (0x13)   Service number
**             lr2      fileno      File descriptor
**             lr3      buffptr     A pointer to buffer area
**             lr4      nbytes      Number of bytes to be read
**
** Returns:    gr96     count       * See HIF spec
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

INT32
hif_read(lr2, lr3, lr4, gr96)
UINT32	lr2, lr3, lr4;
UINT32	*gr96;
   {
   int     bytes_read;
   int       file_no;
   ADDR32    buffptr;
   UINT32     nbytes;
   UINT32     copy_count;
   UINT32     total_bytes;
   INT32       result;

   file_no = (int) lr2;
   buffptr = (ADDR32) lr3;
   nbytes = (UINT32) lr4;
   total_bytes = (UINT32) 0;


   while (nbytes > 0) {

      copy_count = (nbytes < (UINT32) TMP_BUF_SIZE) ? nbytes : (UINT32) TMP_BUF_SIZE;

      bytes_read = (int) read(file_no,
                        (char *) tmp_buffer,
                        (int) copy_count);

      if (bytes_read == (int) -1) {
        *gr96 = total_bytes;
         return((INT32) errno);
      }

      /* Write data to target buffer */
      result = UDI_write_string((INT32) UDI29KDRAMSpace,
                            buffptr,
                            (INT32) bytes_read,
                            tmp_buffer);

      if (result != (INT32) 0) {
        *gr96 = total_bytes;
         return((INT32) result);
      }

      total_bytes = total_bytes + (UINT32) bytes_read;
      buffptr = buffptr + (UINT32) bytes_read;

      /* If bytes_read is the same a copy_count, keep reading */
      if ((UINT32) bytes_read == copy_count)
         nbytes = nbytes - (UINT32) bytes_read;
         else
            nbytes = 0;

   }  /* end while loop */

   *gr96 = total_bytes;
   return (0);

   }  /* end hif_read() */



/*
** Service 20 - write
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    20 (0x14)   Service number
**             lr2      fileno      File descriptor
**             lr3      buffptr     A pointer to buffer area
**             lr4      nbytes      Number of bytes to be read
**
** Returns:    gr96     count       * See HIF spec
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

INT32
hif_write(lr2, lr3, lr4, gr96)
UINT32	lr2, lr3, lr4;
UINT32	*gr96;
    {
    int     file_no;
    ADDR32  buffptr;
    UINT32   nbytes;
    UINT32   copy_count;
    UINT32   total_bytes;
    int     bytes_written;
    INT32     result;

   file_no = (int) lr2;
   buffptr = (ADDR32) lr3;
   nbytes = (UINT32) lr4;
   total_bytes = (UINT32) 0;


   /*   If we are writing channel 0, which is stdin
	this is an error  */

   while (nbytes > (UINT32) 0) {

      copy_count = (nbytes < (UINT32) TMP_BUF_SIZE) ? nbytes : (UINT32) TMP_BUF_SIZE;

      result = UDI_read_string((UINT32) UDI29KDRAMSpace,
                            buffptr,
                            copy_count,
                            tmp_buffer);

      if (result != (INT32) 0) {
        *gr96 = total_bytes;
         return(result);
      }

      /* Write copy_count bytes to file_no */
      bytes_written = write(file_no, tmp_buffer, (int) copy_count);

      if (bytes_written < 0) {
        *gr96 = total_bytes;
         return(errno);
      }

      total_bytes = total_bytes + bytes_written;
      buffptr = buffptr + bytes_written;
      nbytes = nbytes - bytes_written;

   }  /* end while loop */


   *gr96 = total_bytes;

   return (0);
   }  /* end hif_write() */



/*
** Service 21 - lseek
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    21 (0x15)   Service number
**             lr2      file_no      File descriptor
**             lr3      offset      Number of bytes offset from orig
**             lr4      orig        A code number indication the point
**                                  within the file from which the
**                                  offset is measured
**
** Returns:    gr96     where       * See HIF spec
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Notes:  The defined constants for orig are different in MS-DOS
**         and UNIX, but there is a 1-1 correspondence as follows:
**
**       Code  UNIX    MS-DOS    Meaning
**       ----  ----    ------    -------
**         0   L_SET   SEEK_SET  Beginning of file
**         1   L_INCR  SEEK_CUR  Current file position
**         2   L_XTNX  SEEK_END  End of file
**
*/

INT32
hif_lseek(lr2, lr3, lr4, gr96)
UINT32	lr2, lr3, lr4;
UINT32	*gr96;
   {
   int    file_no;
   off_t  offset;
   int    orig;
   long   new_offset;

   file_no = (int) lr2;
   offset = (off_t) lr3;
#ifdef HAVE_UNISTD_H
   if (lr4 == (UINT32) 0)
     orig = SEEK_SET;
   else if (lr4 == (UINT32) 1)
     orig = SEEK_CUR;
   else if (lr4 == (UINT32) 2)
     orig = SEEK_END;
#else
   if (lr4 == (UINT32) 0)
     orig = L_SET;
   else if (lr4 == (UINT32) 1)
     orig = L_INCR;
   else if (lr4 == (UINT32) 2)
     orig = L_XTND;
#endif

   new_offset = lseek(file_no, offset, orig);

   *gr96 = (UINT32) new_offset;

   if (new_offset == (UINT32) -1)  /* failed */
      return(errno);

   return (0);
   }  /* end hif_lseek() */




/*
** Service 22 - remove
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    22 (0x16)   Service number
**             lr2      pathname    A pointer to string that contains
**                                  the pathname of the file
**
** Returns:    gr96     retval      Success = 0
**                                  Failure < 0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

INT32
hif_remove(lr2, gr96)
UINT32	lr2;
UINT32	*gr96;
   {
   int      retval;
   INT32      result;


   result = UDI_read_string((INT32) UDI29KDRAMSpace,
                        (ADDR32) lr2,
                        (INT32) MAX_FILENAME,
                        tmp_buffer);

   if (result != (INT32) 0) {
     *gr96 = (UINT32) -1;
      return(result);
   }

   retval = unlink(tmp_buffer);

   if (retval != 0) {
     *gr96 = (UINT32) -1;
      return(errno);
   }

   *gr96 = (UINT32) 0;
   return (0);

   }  /* end hif_remove() */



/*
** Service 23 - rename
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    23 (0x17)   Service number
**             lr2      oldfile     A pointer to string containing
**                                  the old pathname of the file
**             lr3      newfile     A pointer to string containing
**                                  the new pathname of the file
**
** Returns:    gr96     retval      Success = 0
**                                  Failure < 0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

INT32
hif_rename(lr2, lr3, gr96)
UINT32	lr2, lr3;
UINT32	*gr96;
   {
   char     oldname[MAX_FILENAME];
   int      retval;
   INT32      result;


   /* Get old filename */
   result = UDI_read_string((INT32) UDI29KDRAMSpace,
                        (ADDR32) lr2,
                        (INT32) MAX_FILENAME,
                        oldname);

   if (result != (INT32) 0) {
      *gr96 = (UINT32) -1;
      return(result);
   }

   /* Get new filename */
   result = UDI_read_string((INT32) UDI29KDRAMSpace,
                        (ADDR32) lr3,
                        (INT32) MAX_FILENAME,
                        tmp_buffer);

   if (result != (INT32) 0) {
      *gr96 = (UINT32) -1;
      return(result);
   }

   retval = rename(oldname, tmp_buffer);

   *gr96 = (UINT32) retval;

   if (retval < 0) {
      return(errno);
   }


   return (0);

   }  /* end hif_rename() */



/*
** Service 24 - ioctl
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    24 (0x18)   Service number
**             lr2      fileno      File descriptor number to be tested
**             lr3      mode        Operating mode
**
** Returns:    gr96     retval      Success = 0
**                                  Failure < 0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Note:  There is no equivalent to ioctl() in MS-DOS.  It is
**        stubbed to return a zero.
*/

INT32
hif_ioctl(lr2, lr3)
UINT32	lr2, lr3;
   {
   int   des;
   int   request;
   int   result;


   des = (int) lr2;
   request = (int) lr3;

   result = ioctl(des, request);

   if (result == -1)
      return(errno);

   return (0);

   }  /* end hif_ioctl() */



/*
** Service 25 - iowait
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    25 (0x19)   Service number
**             lr2      fileno      File descriptor number to be tested
**             lr3      mode        1 = non-blocking completion test
**                                  2 = wait until read operation complete
**
** Returns:    gr96     count       * see HIF spec
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Note:  As with ioctl(), there is no equivalent to iowait() in
**        MS-DOS.  It is stubbed to return a zero.
*/

/* ARGSUSED */
INT32
hif_iowait(lr2, lr3)
UINT32	lr2, lr3;
   {
   return (HIF_EHIFNOTAVAIL);
   }  /* end hif_iowait() */



/*
** Service 26 - iostat
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    26 (0x20)   Service number
**             lr2      fileno      File descriptor number to be tested
**
** Returns:    gr96     iostat      input status
**                                  0x0001 = RDREADY
**                                  0x0002 = ISATTY
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Note:  Currently RDREADY is always returned as set.  This is
**        ok for MS-DOS, but may cause problems in BSD UNIX.
**
*/

/* ARGSUSED */
INT32
hif_iostat(lr2, gr96)
UINT32	lr2;
UINT32	*gr96;
   {
   UDIError  result;
   UINT32  file_no;


   *gr96 = (UINT32) RDREADY;

   file_no = lr2;

   result = (UDIError) isatty((int) file_no);

   if (result == (UDIError) 0)
      *gr96 = (UINT32) (*gr96 | ISATTY);

   return (0);

   }  /* end hif_iostat() */


/*
** Service 33 - tmpnam
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    33 (0x21)   Service number
**             lr2      addrptr     Pointer into which filename is
**                                  to be stored
**
** Returns:    gr96     filename    Success: pointer to temporary
**                                  filename string.  This will be
**                                  the same as lr2 on entry unless
**                                  an error occurred
**                                  Failure: = 0 (NULL pointer)
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Warnings:  This function does not check environment variables
**            such as TMP when creating a temporary filename.
**
**            Also, an input parameter of NULL is not accepted.  This
**            would require allocation of a temporary buffer on the
**            target for storage of the temporary file name.  The
**            target must necessarily specify a buffer address for the
**            temporary filename.
**
*/

INT32
hif_tmpnam(lr2, gr96)
UINT32	lr2;
UINT32	*gr96;
   {
   ADDR32  addrptr;
   char   *filename;
   INT32     result;


   /*
   ** If addrptr is zero, there is supposed to be a temporary
   ** buffer allocated on the target.  Since we can't allocate
   ** memory on the target we have to return an error.  This
   ** should be fixed.
   */

   addrptr = lr2;

   if (addrptr == (UINT32) 0) {
     *gr96 = (UINT32) 0;
      return(HIF_EACCESS);
   }

   filename = tmpnam(tmp_buffer);

   if (filename == NULL) {
     *gr96 = (UINT32) 0;
      return(HIF_EACCESS);
   }

   result = UDI_write_string((INT32) UDI29KDRAMSpace,
                         addrptr,
                         (INT32) (strlen(filename) + 1),
                         filename);
   if (result != (INT32) 0) {
      *gr96 = (UINT32) 0;
      return(result);
   }

   *gr96 = (UINT32) addrptr;

   return (0);

   }  /* end hif_tmpnam() */



/*
** Service 49 - time
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    49 (0x31)   Service number
**
** Returns:    gr96     secs        Success != 0 (time in seconds)
**                                  Failure = 0
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
*/

/* ARGSUSED */
INT32
hif_time(gr96)
UINT32	*gr96;
   {
   time_t  secs;


   secs = time((time_t *) 0);

   *gr96 = (UINT32) secs;

   return (0);

   }  /* end hif_time() */



/*
** Service 65 - getenv
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    65 (0x41)   Service number
**             lr2      name        A pointer to symbol name
**		lr3	destination - given by OS.
**
** Returns:    gr96     addrptr     Success: pointer to symbol name string
**                                  Failure = 0 (NULL pointer)
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
** Note:  Since this service requires writing to a buffer on the
**        target, an extra parameter has been passed in lr3.
**        This parameter points to a buffer which can be used
**        by getenv.
*/

INT32
hif_getenv(lr2, lr3, gr96)
UINT32	lr2;
UINT32	lr3;
UINT32	*gr96;
   {
   char   *varval;
   INT32     result;

   result = UDI_read_string((INT32) UDI29KDRAMSpace,
                        (ADDR32) lr2,
                        (INT32) MAX_ENV,
                        tmp_buffer);

   if (result != (INT32) 0) {
      *gr96 = (UINT32) 0;
      return(result);
   }

   varval = (char *) getenv(tmp_buffer);

   if (varval == NULL)
     result = UDI_write_string((INT32) UDI29KDRAMSpace,
                         (ADDR32) lr3,
                         (INT32) 4,
                         "\0\0\0\0");
   else
     result = UDI_write_string((INT32) UDI29KDRAMSpace,
                         (ADDR32) lr3,
                         (INT32) (strlen(varval) + 1),
                         varval);

   if (result != (INT32) 0) {
      *gr96 = (UINT32) 0;
      return(result);
   }

   *gr96 = lr3;

   return (0);

   }  /* end hif_getenv() */




/*
** Service 66 - gettz
**
** Type         Regs    Contents    Description
** ----         ----    --------    -----------
** Calling:    gr121    66 (0x42)   Service number
**
** Returns:    gr96     zonecode    Success >= 0 (minutes west of GMT)
**                                  Failure < 0 (or information
**                                               unavailable)
**             gr97     dstcode     Success = 1 (Daylight Savings Time
**                                               in effect)
**                                          = 0 (Daylight Savings Time
**                                               not in effect)
**             gr121    0x80000000  Logical TRUE, service successful
**                      errcode     error number, service not
**                                  successful
**
*/

/* ARGSUSED */
INT32
hif_gettz(gr96, gr97)
UINT32	*gr96;
UINT32	*gr97;
   {
   struct timeb timeptr;


   (void) ftime(&timeptr);

   *gr96 = (UINT32) timeptr.timezone;
   *gr97 = (UINT32) timeptr.dstflag;

   return (0);

   }  /* end hif_gettz() */



/*
** This function is used to read data from the target.
** This function returns zero if successful, and an
** error code otherwise.
**
** Note that this function permits reading any
** arbitrary sized buffer on the target into a
** buffer on the host.
*/

INT32
UDI_read_string(memory_space, address, byte_count, data)
   INT32   memory_space;
   ADDR32  address;
   INT32   byte_count;
   char   *data;
   {
   UDIResource	from;
   UDICount	count_done;
   UDIError	UDIretval;

   from.Offset = address;
   from.Space = (CPUSpace) memory_space;

   if ((UDIretval = UDIRead (from,
			     (UDIHostMemPtr) data,
			     (UDICount) byte_count,
			     (size_t) 1,
			     &count_done,
			     (UDIBool) 0)) != UDINoError) {
      com_error = 1;
      return (UDIretval);
   };
   if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
     /* 
      * Examine UDIretval and send a GO to switch the Debugger context
      * back to HIF kernel.
      */
      Mini_build_go_msg();
      if (Mini_msg_send() != SUCCESS) {
	com_error = 1;
        return (-1);	/* FAILURE */
      }
   } else { /* old HIF kernel */
   }
    return(0); /* SUCCESS */
   }  /* end read_string() */


/*
** This function is used to write a buffer of data to
** the target.  This function returns zero if successful,
** and an error code otherwise.
**
** Note that this function permits writing any
** arbitrary sized buffer on the target from a
** buffer on the host.
*/

INT32
UDI_write_string(memory_space, address, byte_count, data)
   INT32   memory_space;
   ADDR32  address;
   INT32   byte_count;
   char   *data;
   {
   UDIResource	to;
   UDICount	count_done;
   UDIError	UDIretval;

   to.Offset = address;
   to.Space = (CPUSpace) memory_space;

   if ((UDIretval = UDIWrite ((UDIHostMemPtr) data,
			      to,
			      (UDICount) byte_count,
			      (size_t) 1,
			      &count_done,
			      (UDIBool) 0)) != UDINoError) {
      com_error = 1;
      return (UDIretval);
   }
   if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
     /* 
      * Examine UDIretval and send a GO to switch the Debugger context
      * back to HIF kernel.
      */
      Mini_build_go_msg();
      if (Mini_msg_send() != SUCCESS) {
	com_error = 1;
        return (-1);	/* FAILURE */
      }
   } else { /* old HIF kernel */
   }
    return(0); /* SUCCESS */
   }  /* end UDI_write_string() */


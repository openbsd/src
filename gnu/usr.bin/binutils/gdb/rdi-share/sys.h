/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* sys.h 
 ***********************************************************************
 * Angel C Libary support channel protocol definitions
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 *
 *
 *
 * MESSAGE FORMAT
 * --------------
 * Format of the "data" section of C Lib Support Channel Messages.
 * You will notice that the format is much the same as the format
 * of ADP messages - this is so that multi-threaded C Libraries can
 * be supported.
 *
 *  unsigned32 reason     - Main C Library reason code.
 *  unsigned32 debugID    - Info. describing host debug world;
 *                          private to host and used in any target
 *                          initiated messages.
 *  unsigned32 OSinfo1    \ Target OS information to identify process/thread
 *  unsigned32 OSinfo2    / world, etc. These two fields are target defined.
 *  byte       args[n]    - Data for message "reason" code.
 *
 * The "debugID" is defined by the host-end of the protocol, and is used
 * by the host to ensure that messages are routed to the correct handler
 * program/veneer (eg. imagine several threads having opened stdout and
 * each writing to a different window in a windowed debugger).
 *
 * NOTE: The reason that there is no "size" information, is that the
 * message IDs themselves encode the format of any arguments.
 *
 * For further discussion of the format see adp.h
 *
 * N.B. All streams are little endian.
 *
 * CLIB REASON CODE
 * ----------------
 * The message reason codes contain some information that ties them to
 * the channel and direction that the message will be used with. This
 * will ensure that even if the message "#define name" is not
 * completely descriptive, the message reason code is.
 *
 *      b31    = direction. 0=Host-to-Target; 1=Target-to-Host;
 *      b30-16 = reserved. should be zero
 *      b15-0  = message reason code.
 *
 * Note that typically a request will be initiated by the target side, and
 * that the host will then respond with either an acknowledgement or some
 * data.  In either case the same reason code will be used, but the direction
 * bit will be reveresed.
 */

#ifndef __sys_h
#define __sys_h

#ifndef HtoT
#define HtoT    ((unsigned)0 << 31)     /* Host-to-Target message */
#define TtoH    ((unsigned)1 << 31)     /* Target-to-Host message */
#endif

/*
 * The following are error codes used in the status field returned on
 * sending a message. 0 represents no error having occurred, non-zero
 * represents a general error.  More codes should be added as required.
 */
 
#ifndef ErrCode
#define NoError  0x0
#endif

/*************************************************************************/
/* The following are direct conversions of the DeMon SWI's               */
/* NB: nbytes is the number of bytes INCLUDING THE NULL character where  */
/*     applicable.                                                       */

/* This message is used as a  response to a packet whose message
 * was not understood.  The return parameter, code is the reason
 * code which was not understood. Although intended for use as a
 * default case on a received message switch it can also be used
 * as a  proper message*/
#define CL_Unrecognised          0x00
    /* Unrecognised()
     * return(word code)
     */

/* Write a character to the terminal.
 */
#define CL_WriteC       0x01
   /* WriteC(byte data)
    * return(word status)
    */

/* Write a NULL terminated string of characters to the terminal.  The length
 * of the string excluding the NULL terminating character is passed in 
 * 'nbytes'.
 */
#define CL_Write0       0x02
   /* Write0(word nbytes, bytes data)
    * return(word status)
    */

/* Read a character from the terminal - probably the keyboard.
 */
#define CL_ReadC        0x04
   /* ReadC(void)
    * return(word status, byte data)
    */

/* Perform system call, pass NULL terminated string to host's command
 * line interpreter(NOT AVAILABLE IN PC/DOS RELEASE).  The data byte
 * returned holds the return code from the system call.
 */ 
#define CL_System       0x05
   /* CLI(word nbytes, bytes data)
    * return(word status, word data)
    */

/* It returns the address of the null terminated command line string used to
 * invoke the program. status will be set to NoError if the command line
 * can be returned. Other status values will be treated as error conditions.
 */
#define CL_GetCmdLine   0x10
   /* GetCmdLine(void)
    * return(word status, word nbytes, bytes argline)
    */

/* Return the number of centi-seconds since the support code began 
 * execution.  Only the difference between successive calls can be
 * meaningful.
 */
#define CL_Clock        0x61
   /* Clock(void)
    * return(word status, word clks)
    */

/* Return the number of seconds since the beginning of 1970.
 */
#define CL_Time         0x63
   /* Time(void)
    * return(word status, word time)
    */

/* Delete(remove, un-link, wipe, destroy) the file named by the
 * NULL-terminated string 'name'.
 */
#define CL_Remove       0x64
   /* Remove(word nbytes, bytes name)
    * return(word status)
    */

/* Rename the file specified by the NULL-terminated string 'oname'
 * to 'nname'.
 */   
#define CL_Rename       0x65
   /* Rename(word nbytes, bytes oname, word nbytes, bytes nname)
    * return(word status)
    */

/* 'name' specifies a NULL-terminated string containing a file name or a
 * device name.  Opens the file/device and returns a non-zero handle on
 * success that can be quoted to CL_Close, CL_Read, CL_Write, CL_Seek,
 * CL_Flen or CL_IsTTY.  The mode is an integer in the range 0-11:-
 *
 * Mode:              0   1   2   3   4   5   6   7   8   9   10   11
 * ANSI C fopen mode: r   rb  r+  r+b w   wb  w+  w+b a   ab  a+   a+b
 *
 * Values 12-15 are illegal.  If 'name' is ":tt" the stdin/stdout is
 * opened depending on whether 'mode' is read or write.
 */ 
#define CL_Open         0x66
   /* Open(word nbytes, bytes name, word mode)
    * return(word handle)
    */

/* 'handle' is a file handle previously returned by CL_Open.  CL_Close
 * closes the file.
 */ 
#define CL_Close        0x68
   /* Close(word handle)
    * return(word status)
    */

/* Writes data of length nbytes to the file/device specified by
 * handle.  nbtotal represents the total number of bytes to be
 * written, whereas nbytes is the number of bytes in this packet
 *
 * If nbtotal is <= DATASIZE - CL_Write message header size in the
 * packet then nbytes = nbtotal and the number of bytes not written
 * is returned.  If nbtotal is > the packet size then the CL_Write
 * must be followed by a number of CL_WriteX's to complete the write,
 * the nbytes returned by CL_Write can be ignored
 * If the status word returned is non zero, an error has occurred and
 * the write request has been aborted.
 * 
 */
#define CL_Write        0x69
   /* Write(word handle, word nbtotal, word nbytes, bytes data)
    * return(word status, word nbytes)
    */

/* Write Extension is a reads a continuation of data from a CL_Write 
 * which was too big to fit in a single packet.
 * nbytes is the number of bytes of data in this packet, the 
 * returned value of nbytes can be ignored except if it is the 
 * last packet, in which case it is the number of bytes that were NOT
 * written 
 */
#define CL_WriteX       0x6A
   /* WriteX(word nbytes, bytes data)
    * return(word status, word nbytes)
    */

/* Reads 'nbytes' from the file/device specified by 'handle'.
 *
 * If nbytes <= DATASIZE then the read will occur in a single packet
 * and the returned value of nbytes will be the number of bytes actually
 * read and nbmore will be 0. If nbytes> DATASIZE then multiple packets
 * will have to be used ie CL_Read followed by 1 or more CL_ReadX
 * packets. In this case CL_Read will return nbytes read in the current
 * packet and nbmore representing how many more bytes are expected to be
 * read
 * If the status word is non zero then the request has completed with an
 * error. If the status word is 0xFFFFFFFF (-1) then an EOF condition
 * has been reached.
 */
#define CL_Read         0x6B
   /* Read(word handle, word nbytes)
    * return(word status, word nbytes, word nbmore, bytes data)
    */

/* Read eXtension returns a continuation of the data that was opened for
 * read in the earlier CL_Read. The return value nbytes is the number of
 * data bytes in the packet, nbmore is the number of bytes more that are
 * expected to be read in subsequent packets.
 */
#define CL_ReadX        0x6C
  /* ReadX()
   * return(word status, word nbytes, word nbmore, bytes data)
   */

/* Seeks to byte position 'posn' in the file/device specified by 'handle'.
 */
#define CL_Seek         0x6D
   /* Seek(word handle, word posn)
    * return(word status)
    */

/* Returns the current length of the file specified by 'handle' in 'len'.
 * If an error occurs 'len' is set to -1. 
 */ 
#define CL_Flen         0x6E
   /* Flen(word handle)
    * return(word len)
    */

/* Returns NoError if 'handle' specifies an interactive device, otherwise
 * returns GenError
 */ 
#define CL_IsTTY        0x6F
   /* IsTTY(word handle)
    * return(word status)
    */

/* Returns a temporary host file name. The maximum length of a file name
 * is passed to the host. The TargetID is some identifier from the target
 * for this particular temporary filename. This value is could be used
 * directly in the generation of the filename. 
 *
 * If the host cannot create a suitable name or the generated name is too
 * long then status is non zero. status will be NoError if the host can create
 * a name. 
 */
#define CL_TmpNam       0x70
   /* TmpNam(word maxlength, word TargetID)
    * return(word status, word nbytes, bytes fname)
    */

/* Note there is no message for Exit, EnterOS, InstallHandler or
 * GenerateError as these will be supported entirely at the host end,
 * or by the underlying Operating system.
 */

#define CL_UnknownReason (-1)

extern unsigned int GetRaiseHandler( void );
extern unsigned int SysLibraryHandler(unsigned int sysCode, unsigned int *args);
extern void angel_SysLibraryInit(void);

/*
 * Function: Angel_IsSysHandlerRunning
 *  Purpose: return whether or not SysLibraryHandler is running
 *
 *   No paramaters
 *             
 *   Returns 1 if SysLibraryHandler is running
 *           0 otherwise
 */
extern int Angel_IsSysHandlerRunning(void);

#ifdef ICEMAN2
/* This function exists in an ICEman2 system only, and can be called by
 * debug support code when the debugger tells it how much memory the
 * target has.  This will then be used to deal with the HEAPINFO SWI
 */
extern void angel_SetTopMem(unsigned addr);
#endif

#endif


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
#ifndef angsd_unixcomm_h
#define angsd_unixcomm_h

#include <errno.h>

#if defined(BSD)
#  define ERRNO_FOR_BLOCKED_IO EWOULDBLOCK
#else
#  define ERRNO_FOR_BLOCKED_IO EAGAIN
#endif

/*
 *  Function: Unix_MatchValidSerialDevice
 *   Purpose: check that the serial driver/port name is valid
 *            and return the actual device name if it is.
 *
 *    Params:
 *       Input: name    Name of device going to be used
 *
 *   Returns: 
 *          OK: Pointer to name of the device matched
 *       Error or unrecognised deivce: 0
 */
extern const char *Unix_MatchValidSerialDevice(const char *name);

/*
 *  Function: Unix_IsSerialInUse
 *   Purpose: check whether the serial port is in use
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: 
 *          OK: 0       Serial device not in use
 *       Error: -1      Serial device in use
 */
extern int Unix_IsSerialInUse(void);

/*
 *  Function: Unix_OpenSerial
 *   Purpose: open the serial port
 *
 *    Params:
 *       Input: name    Name of device to open
 *
 *   Returns: Unix 'open' returns
 */
extern int Unix_OpenSerial(const char *name);

/*
 *  Function: Unix_CloseSerial
 *   Purpose: close the serial port
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: Nothing
 */
extern void Unix_CloseSerial(void);

/*
 *  Function: Unix_ReadSerial
 *   Purpose: reads a specified number of bytes (or less) from the serial port
 *
 *    Params:
 *       Input: buf     Buffer to store read bytes
 *              n       Maximum number of bytes to read
 *
 *   Returns: Unix 'read' returns
 */
extern int Unix_ReadSerial(unsigned char *buf, int n, bool block);

/*
 *  Function: Unix_WriteSerial
 *   Purpose: writes a specified number of bytes (or less) to the serial port
 *
 *    Params:
 *       Input: buf     Buffer to write bytes from
 *              n       Maximum number of bytes to write
 *
 *   Returns: Unix 'write' returns
 */
extern int Unix_WriteSerial(unsigned char *buf, int n);

/*
 *  Function: Unix_ResetSerial
 *   Purpose: resets the serial port for another operation
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: Nothing
 */
extern void Unix_ResetSerial(void);

/*
 *  Function: Unix_SetSerialBaudRate
 *   Purpose: check that the serial driver/port name is valid
 *
 *    Params:
 *       Input: baudrate    termios value for baud rate
 *
 *   Returns: Nothing
 */
extern void Unix_SetSerialBaudRate(int baudrate);

/*
 *  Function: Unix_ioctlNonBlocking
 *   Purpose: sets the serial port to non-blocking IO
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: Nothing
 */
extern void Unix_ioctlNonBlocking(void);

/*
 *  Function: Unix_IsValidParallelDevice
 *   Purpose: check whether the combined serial and parallel device specification
 *            is ok, and return the ports selected
 *
 *    Params:
 *       Input: portstring - is a string which specifies which serial
 *                           and parallel ports are to be used. Can
 *                           include s=<val> and p=<val> separated by a
 *                           comma.
 *
 *   Returns: 
 *       Output: *sername  - returns the device name of the chosen serial port
 *               *parname  - returns the device name of the chosen parallel port
 *               If either of these is NULL on return then the match failed.
 */
extern void Unix_IsValidParallelDevice(
  const char *portstring, char **sername, char **parname
);

/*
 *  Function: Unix_IsParallelInUse
 *   Purpose: check whether the parallel port is in use
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: 
 *          OK: 0       Parallel device not in use
 *       Error: -1      Parallel device in use
 */
extern int Unix_IsParallelInUse(void);

/*
 *  Function: Unix_OpenParallel
 *   Purpose: open the parallel port
 *
 *    Params:
 *       Input: name    Name of device to open
 *
 *   Returns: Unix 'open' returns
 */
extern int Unix_OpenParallel(const char *name);

/*
 *  Function: Unix_CloseParallel
 *   Purpose: close the parallel port
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: Nothing
 */
extern void Unix_CloseParallel(void);

/*
 *  Function: Unix_WriteParallel
 *   Purpose: writes a specified number of bytes (or less) to the parallel port
 *
 *    Params:
 *       Input: buf     Buffer to write bytes from
 *              n       Maximum number of bytes to write
 *
 *   Returns: Unix 'write' returns
 */
extern unsigned int Unix_WriteParallel(unsigned char *buf, int n);

/*
 *  Function: Unix_ResetParallel
 *   Purpose: resets the parallel port for another operation
 *
 *    Params:
 *       Input: Nothing
 *
 *   Returns: Nothing
 */
extern void Unix_ResetParallel(void);

#endif /* ndef angsd_unixcomm_h */

/* EOF unixcomm.h */

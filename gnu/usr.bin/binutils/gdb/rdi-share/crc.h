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
 * crc.h - describes some "standard" CRC calculation routines.
 */
#ifndef angel_crc_h
#define angel_crc_h

/*
 * manifests
 */

/*
 * When using "crc32" or "crc16" these initial CRC values must be given to
 * the respective function the first time it is called. The function can
 * then be called with the return value from the last call of the function
 * to generate a running CRC over multiple data blocks.
 * When the last data block has been processed using the "crc32" algorithm
 * the CRC value should be inverted to produce the final CRC value:
 * e.g. CRC = ~CRC
 */

#define startCRC32  (0xFFFFFFFF)    /* CRC initialised to all 1s */
#define startCRC16  (0x0000)        /* CRC initialised to all 0s */

/*
 * For the CRC-32 residual to be calculated correctly requires that the CRC
 * value is in memory little-endian due to the byte read, bit-ordering
 * nature of the algorithm.
 */
#define CRC32residual   (0xDEBB20E3)    /* good CRC-32 residual */


/**********************************************************************/

/*
 * exported functions
 */

/*
 *  Function: crc32
 *   Purpose: Provides a table driven implementation of the IEEE-802.3
 *            32-bit CRC algorithm for byte data.
 *
 *    Params:
 *       Input: address     pointer to the byte data
 *              size        number of bytes of data to be processed
 *              crc         initial CRC value to be used (can be the output
 *                          from a previous call to this function).
 *   Returns:
 *          OK: 32-bit CRC value for the specified data
 */
extern unsigned int crc32(unsigned char *address, unsigned int size,
                          unsigned int crc);

/**********************************************************************/

/*
 *
 *  Function: crc16
 *   Purpose: Generates a table driven 16-bit CRC-CCITT for byte data
 *
 *    Params:
 *       Input: address     pointer to the byte data
 *              size        number of bytes of data to be processed
 *              crc         initial CRC value to be used (can be the output
 *                          from a previous call to this function).
 *
 *   Returns:
 *          OK: 16-bit CRC value for the specified data
 */
extern unsigned short crc16(unsigned char *address, unsigned int size,
                            unsigned short crc);

/**********************************************************************/

#endif /* !defined(angel_crc_h) */

/* EOF crc.h */

/*	$OpenBSD: hifn7751reg.h,v 1.1 1999/02/19 02:52:20 deraadt Exp $	*/

/*
 * Invertex AEON driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 *
 * Please send any comments, feedback, bug-fixes, or feature requests to
 * software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __AEON_H__
#define __AEON_H__

#include <machine/endian.h>

/*
 * Some PCI configuration space offset defines.  The names were made
 * identical to the names used by the Linux kernel.
 */
#define  PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define  PCI_BASE_ADDRESS_1	0x14	/* 32 bits */

/*
 *  Some configurable values for the driver
 */
#define AEON_DESCRIPT_RING_SIZE  24
#define AEON_MAX_DEVICES          4

/*
 * The values below should multiple of 4 -- and be large enough to handle
 * any command the driver implements.
 */
#define AEON_MAX_COMMAND_LENGTH 120
#define AEON_MAX_RESULT_LENGTH   16

/*
 * aeon_descriptor_t
 *
 * Holds an individual descriptor for any of the rings.
 */
typedef struct aeon_descriptor {
	volatile u_int32_t length;  /* length and status bits */
	volatile u_int32_t pointer;
} aeon_descriptor_t;

/*
 * Masks for the "length" field of struct aeon_descriptor.
 */
#define AEON_DESCRIPT_MASK_DONE_IRQ (0x1 << 25)
#define AEON_DESCRIPT_LAST          (0x1 << 29)
#define AEON_DESCRIPT_JUMP          (0x1 << 30)
#define AEON_DESCRIPT_VALID         (0x1 << 31)

/*
 * aeon_callback_t 
 *
 * Type for callback function when dest data is ready.
 */
typedef void (*aeon_callback_t)(aeon_command_t *);

/*
 * Data structure to hold all 4 rings and any other ring related data.
 */
struct aeon_dma {
	/*
	 *  Descriptor rings.  We add +1 to the size to accomidate the
	 *  jump descriptor.
	 */
	struct aeon_descriptor	command_ring[AEON_DESCRIPT_RING_SIZE + 1];
	struct aeon_descriptor	source_ring[AEON_DESCRIPT_RING_SIZE + 1];
	struct aeon_descriptor	dest_ring[AEON_DESCRIPT_RING_SIZE + 1];
	struct aeon_descriptor	result_ring[AEON_DESCRIPT_RING_SIZE + 1];

	aeon_command_t	*aeon_commands[AEON_DESCRIPT_RING_SIZE ];

	u_char	command_bufs[AEON_DESCRIPT_RING_SIZE][AEON_MAX_COMMAND_LENGTH];
	u_char	result_bufs[AEON_DESCRIPT_RING_SIZE][AEON_MAX_RESULT_LENGTH];

	/*
	 *  Our current positions for insertion and removal from the desriptor
	 *  rings. 
	 */
	u_int32_t ring_pos;
	u_int32_t wakeup_ring_pos;
	volatile u_int32_t slots_in_use;
};

/*
 * Holds data specific to a single AEON board.
 */
struct aeon_softc {
	struct device	sc_dv;		/* generic device */
	void *		sc_ih;		/* interrupt handler cookie */
	u_int32_t is_dram_model;	/* 1=dram, 0=sram */

	/* Register set 0 */
	bus_space_handle_t	sc_sh0;
	bus_space_tag_t		sc_st0;

	/* Register set 1 */
	bus_space_handle_t	sc_sh1;
	bus_space_tag_t		sc_st1;

	struct aeon_dma *sc_dma;
};

/*
 * Register offsets in register set 0
 */
#define AEON_INIT_1			0x04
#define AEON_RAM_CONFIG			0x0c
#define AEON_EXPAND			0x08
#define AEON_ENCRYPTION_LEVEL		0x14
#define AEON_INIT_3			0x10
#define AEON_INIT_2			0x1c

#define WRITE_REG_0(sc,reg,val) \
    bus_space_write_4((sc)->sc_st0, (sc)->sc_sh0, reg, val)
#define READ_REG_0(sc,reg) \
    bus_space_read_4((sc)->sc_st0, (sc)->sc_sh0, reg)

/*
 * Register offsets in register set 1
 */
#define AEON_COMMAND_RING_ADDR		0x0c
#define AEON_SOURCE_RING_ADDR		0x1c
#define AEON_RESULT_RING_ADDR		0x2c
#define AEON_DEST_RING_ADDR		0x3c
#define	AEON_STATUS			0x40
#define	AEON_INTERRUPT_ENABLE		0x44

#define	AEON_DMA_CFG			0x48
#define AEON_DMA_CFG_NOBOARDRESET	0x00000001
#define AEON_DMA_CFG_NODMARESET		0x00000002
#define AEON_DMA_CFG_NEED		0x00000004
#define AEON_DMA_CFG_HOSTLAST		0x00000010

#define WRITE_REG_1(sc,reg,val)	\
    bus_space_write_4((sc)->sc_st1, (sc)->sc_sh1, reg, val)
#define READ_REG_1(sc,reg) \
    bus_space_read_4((sc)->sc_st1, (sc)->sc_sh1, reg)

/*
 * Initial register values
 */

/*
 *  Status Register
 *  
 *  The value below enables polling on all 4 descriptor rings and
 *  writes a "1" to every status bit in the register.  (Writing "1"
 *  clears the bit.)
 */
#define AEON_INIT_STATUS_REG		((1<<31)|(1<<23)|(1<<15)|(1<<7))

/*
 *  Interrupt Enable Register 
 *
 *  Initial value sets all interrupts to off except the "mask done"
 *  interrupt of the the result descriptor ring.
 */
#define	AEON_INIT_INTERRUPT_ENABLE_REG	(AEON_INTR_ON_RESULT_DONE)

/*
 *  DMA Configuration Register
 *  
 *  Initial value sets the polling scalar and frequency, and puts
 *  the host (not the AEON board) in charge of "last" bits in the
 *  dest data and result descriptor rings.
 */
#define	AEON_INIT_DMA_CONFIG_REG					   \
    (AEON_DMA_CFG_NOBOARDRESET | AEON_DMA_CFG_NODMARESET | \
    AEON_DMA_CFG_NEED | \
    AEON_DMA_CFG_HOSTLAST |		/* host controls last bit in all rings */  \
    (AEON_POLL_SCALAR << 8) |		/* setting poll scalar value */		   \
    (AEON_POLL_FREQUENCY << 16))	/* setting poll frequency value */

/*
 *  RAM Configuration Register
 *
 *  Initial value sets the ecryption context size to 128 bytes (if using
 *  RC4 bump it to 512, but you'll decrease the number of available
 *  sessions).  We don't configure multiple compression histories -- since
 *  IPSec doesn't use them.
 *
 *  NOTE:  Use the AEON_RAM_CONFIG_INIT() macro instead of the
 *  variable, since DRAM/SRAM detection is not determined staticly.
 */
#define AEON_INIT_RAM_CONFIG_REG				\
  ((0x0 << 1) |		/* RAM Encrypt: 0 for 128 bytes, 1 for 512 bytes */  \
   (0x1 << 2) |		/* RAM Comp cfg: 1 for single compression history */ \
   0x4B40)		/* Setting fixed bits required by the register */

/*
 *  Expand Register
 *
 *  The only bit in this register is the expand bit at position 9.  It's
 *  cleared by writing a 1 to it.
 */
#define AEON_INIT_EXPAND_REG 	(0x1 << 9)

/*********************************************************************
 * Structs for board commands 
 *
 *********************************************************************/

/*
 * Structure to help build up the command data structure.
 */
typedef struct aeon_base_command {
	u_int16_t masks;
	u_int16_t session_num;
	u_int16_t total_source_count;
	u_int16_t total_dest_count;
} aeon_base_command_t;

#define AEON_BASE_CMD_MAC    (0x1 << 10)
#define AEON_BASE_CMD_CRYPT  (0x1 << 11)
#define AEON_BASE_CMD_DECODE (0x1 << 13)

/*
 * Structure to help build up the command data structure.
 */
typedef struct aeon_crypt_command {
	u_int16_t masks;               
	u_int16_t header_skip;
	u_int32_t source_count;
} aeon_crypt_command_t;

#define AEON_CRYPT_CMD_ALG_MASK  (0x3 << 0)
#define AEON_CRYPT_CMD_ALG_DES   (0x0 << 0)
#define AEON_CRYPT_CMD_ALG_3DES  (0x1 << 0)
#define AEON_CRYPT_CMD_MODE_CBC  (0x1 << 3)
#define AEON_CRYPT_CMD_NEW_KEY   (0x1 << 11)
#define AEON_CRYPT_CMD_NEW_IV    (0x1 << 12)

/*
 * Structure to help build up the command data structure.
 */
typedef struct aeon_mac_command {
	u_int16_t masks;  
	u_int16_t header_skip;
	u_int32_t source_count;
} aeon_mac_command_t;

#define AEON_MAC_CMD_ALG_MD5    (0x1 << 0)
#define AEON_MAC_CMD_ALG_SHA1   (0x0 << 0)
#define AEON_MAC_CMD_MODE_HMAC  (0x0 << 2)
#define AEON_MAC_CMD_TRUNC      (0x1 << 4)
#define AEON_MAC_CMD_APPEND     (0x1 << 6)
/*
 * MAC POS IPSec initiates authentication after encryption on encodes
 * and before decryption on decodes.
 */
#define AEON_MAC_CMD_POS_IPSEC  (0x2 << 8)
#define AEON_MAC_CMD_NEW_KEY    (0x1 << 11)

/*
 * Structure with all fields necessary to write the command buffer.
 * We build it up while interrupts are on, then use it to write out
 * the command buffer quickly while interrupts are off.
 */
typedef struct aeon_command_buf_data {
	aeon_base_command_t base_cmd;
	aeon_mac_command_t mac_cmd;
	aeon_crypt_command_t crypt_cmd;
	const u_int8_t *mac_key;
	const u_int8_t *crypt_key;
	const u_int8_t *initial_vector;
} aeon_command_buf_data_t;

/*
 * Values for the interrupt enable register
 */
#define AEON_INTR_ON_RESULT_DONE     (1 << 20)
#define AEON_INTR_ON_COMMAND_WAITING  (1 << 2)

/*
 * The poll frequency and poll scalar defines are unshifted values used
 * to set fields in the DMA Configuration Register.
 */
#ifndef AEON_POLL_FREQUENCY
#define AEON_POLL_FREQUENCY  0x1
#endif

#ifndef AEON_POLL_SCALAR
#define AEON_POLL_SCALAR    0x0
#endif

#endif /* __AEON_H__ */

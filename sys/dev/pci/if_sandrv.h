/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__IF_SANDRV_H
#   define	__IF_SANDRV_H

#ifdef __SDLADRV__
# define EXTERN 
#else
# define EXTERN extern
#endif



#define WAN_MAILBOX_SIZE	16
#define WAN_MAX_DATA_SIZE	2032
#pragma pack(1)
typedef struct {
	union {
		struct {
			unsigned char  opp_flag;
			unsigned char  command;
			unsigned short data_len;	
			unsigned char  return_code;
		} wan_p_cmd;
		unsigned char mbox[WAN_MAILBOX_SIZE];
	} wan_cmd_u;
#define wan_cmd_opp_flag	wan_cmd_u.wan_p_cmd.opp_flag
#define wan_cmd_command		wan_cmd_u.wan_p_cmd.command
#define wan_cmd_data_len	wan_cmd_u.wan_p_cmd.data_len
#define wan_cmd_return_code	wan_cmd_u.wan_p_cmd.return_code
} wan_cmd_t;
#pragma pack()

/************************************************
 *	GLOBAL DEFINITION FOR SANGOMA MAILBOX	*
 ************************************************/
#pragma pack(1)
typedef struct {
	wan_cmd_t	wan_cmd;
	unsigned char	wan_data[WAN_MAX_DATA_SIZE];
#define wan_opp_flag			wan_cmd.wan_cmd_opp_flag
#define wan_command			wan_cmd.wan_cmd_command
#define wan_data_len			wan_cmd.wan_cmd_data_len
#define wan_return_code			wan_cmd.wan_cmd_return_code
} wan_mbox_t;
#pragma pack()
#define WAN_MBOX_INIT(mbox)	memset(mbox, 0, sizeof(wan_cmd_t));


#if defined(_KERNEL)

/*
******************************************************************
**			D E F I N E S				**	
******************************************************************
*/
#define SDLADRV_MAJOR_VER	2
#define SDLADRV_MINOR_VER	1
#define	SDLA_WINDOWSIZE		0x2000	/* default dual-port memory window size */

/* Adapter types */
#define SDLA_S508	5080
#define SDLA_S514	5140
#define SDLA_ADSL	6000
#define SDLA_AFT	7000

#define SDLA_PRI_PORT	1
#define SDLA_SEC_PORT	2

/* Firmware supported version */
#define	SFM_VERSION	2
#define	SFM_SIGNATURE	"SFM - Sangoma SDLA Firmware Module"

/* min/max */
#define	SFM_IMAGE_SIZE	0x8000	/* max size of SDLA code image file */
#define	SFM_DESCR_LEN	256	/* max length of description string */
#define	SFM_MAX_SDLA	16	/* max number of compatible adapters */

/* Firmware identification numbers:
 *    0  ..  999	Test & Diagnostics
 *  1000 .. 1999	Streaming HDLC
 *  2000 .. 2999	Bisync
 *  3000 .. 3999	SDLC
 *  4000 .. 4999	HDLC
 *  5000 .. 5999	X.25
 *  6000 .. 6999	Frame Relay
 *  7000 .. 7999	PPP
 *  8000 .. 8999        Cisco HDLC
 */
#define	SFID_HDLC502	4200
#define	SFID_HDLC508	4800
#define	SFID_CHDLC508	8800
#define SFID_CHDLC514	8140
#define SFID_AFT       30000

/* */
#define SDLA_MEMBASE		0x01
#define SDLA_MEMEND		0x02
#define SDLA_MEMSIZE		0x03
#define SDLA_MEMORY		0x05
#define SDLA_BASEADDR		0x06
#define SDLA_DMATAG		0x04
#define SDLA_IRQ		0x07
#define SDLA_BUS		0x08
#define SDLA_CPU		0x0A
#define SDLA_SLOT		0x0B
#define SDLA_ADAPTERTYPE	0x0C
#define SDLA_CARDTYPE		0x0D
#define SDLA_PCIEXTRAVER	0x0E

/* S514 PCI adapter CPU numbers */
#define SDLA_MAX_CPUS		2
#define S514_CPU_A		'A'
#define S514_CPU_B		'B'
#define SDLA_CPU_A		1
#define SDLA_CPU_B		2
#define SDLA_GET_CPU(cpu_no)	(cpu_no==SDLA_CPU_A)?S514_CPU_A:S514_CPU_B

#define AFT_CORE_ID_MASK	0x00FF
#define AFT_CORE_REV_MASK	0xFF00
#define AFT_HDLC_CORE_ID	0x00	/* HDLC core */
#define AFT_ATM_CORE_ID		0x01	/* ATM core */
#define AFT_SS7_CORE_ID		0x02	/* SS7 core */

#define XILINX_PCI_MEM_SIZE	0x2FF
#define XILINX_PCI_LATENCY	0x0000FF00

#define XILINX_PCI_CMD_REG	0x04
#define XILINX_PCI_LATENCY_REG  0x0C

/* Local PCI register offsets */ 
#if 0
#define PCI_VENDOR_ID_WORD	0x00		/* vendor ID */
#define PCI_DEVICE_ID_WORD	0x02		/* device ID */
#define PCI_SUBCLASS_ID_BYTE	0x0a		/* subclass ID byte */
#endif
#define PCI_IO_BASE_DWORD	0x10	/* IO base */	
#define PCI_MEM_BASE0_DWORD	0x14	/* memory base - apperture 0 */
#define PCI_MEM_BASE1_DWORD     0x18	/* memory base - apperture 1 */
#if 0
#define PCI_SUBSYS_VENDOR_WORD 	0x2C		/* subsystem vendor ID */
#define PCI_SUBSYS_ID_WORD 	0x2E		/* subsystem ID */
#define PCI_INT_LINE_BYTE	0x3C		/* interrupt line */
#define PCI_INT_PIN_BYTE	0x3D		/* interrupt pin */
#endif
#define PCI_MAP0_DWORD		0x40	/* PCI to local bus address 0 */
#define PCI_MAP1_DWORD          0x44	/* PCI to local bus address 1 */
#define PCI_INT_STATUS          0x48		/* interrupt status */
#define PCI_INT_CONFIG		0x4C		/* interrupt configuration */
  
#define PCI_DEV_SLOT_MASK	0x1F		/* mask for slot numbering */
#define PCI_IRQ_NOT_ALLOCATED	0xFF		/* interrupt line for no IRQ */
/* Local PCI register usage */
#define PCI_MEMORY_ENABLE	0x00000003	/* enable PCI memory */
#define PCI_CPU_A_MEM_DISABLE	0x00000002	/* disable CPU A memory */
#define PCI_CPU_B_MEM_DISABLE  	0x00100002	/* disable CPU B memory */
#define PCI_ENABLE_IRQ_CPU_A	0x005A0004	/* enable IRQ for CPU A */
#define PCI_ENABLE_IRQ_CPU_B    0x005A0008	/* enable IRQ for CPU B */
#define PCI_ENABLE_IRQ_DMA0     0x01000000	/* enable IRQ for DMA 0 */
#define PCI_ENABLE_IRQ_DMA1     0x02000000	/* enable IRQ for DMA 1 */
#define PCI_DISABLE_IRQ_CPU_A   0x00000004	/* disable IRQ for CPU A */
#define PCI_DISABLE_IRQ_CPU_B   0x00000008	/* disable IRQ for CPU B */
#define PCI_DISABLE_IRQ_DMA0     0x01000000	/* disable IRQ for DMA 0 */
#define PCI_DISABLE_IRQ_DMA1     0x02000000	/* disable IRQ for DMA 1 */
 
/* Setting for the Interrupt Status register */  
#define IRQ_DMA0		0x01000000	/* IRQ for DMA0 */
#define IRQ_DMA1		0x02000000	/* IRQ for DMA1 */
#define IRQ_LOCAL_CPU_A         0x00000004	/* IRQ for CPU A */
#define IRQ_LOCAL_CPU_B		0x00000008	/* IRQ for CPU B */
#define IRQ_CPU_A               0x04            /* IRQ for CPU A */
#define IRQ_CPU_B               0x08		/* IRQ for CPU B */

/* The maximum size of the S514 memory */
#define MAX_SIZEOF_S514_MEMORY	(256 * 1024)

/* S514 control register offsets within the memory address space */
#define S514_CTRL_REG_BYTE	0x80000
 
/* S514 adapter control bytes */
#define S514_CPU_HALT 		0x00
#define S514_CPU_START		0x01

/* The maximum number of S514 adapters supported */
#define MAX_S514_CARDS		20	

#define WAN_CMD_OK		0	/* normal firmware return code */
#define WAN_CMD_TIMEOUT		0xFF	/* firmware command timed out */

/* signature: 'SDLA' reversed */
#define	SDLAHW_MAGIC		0x414C4453L

/*
******************************************************************
**			M A C R O S				**
******************************************************************
*/
#define AFT_CORE_ID_DECODE(core_id)			\
		(core_id == AFT_HDLC_CORE_ID) ? "HDLC" :	\
		(core_id == AFT_ATM_CORE_ID) ? "ATM"   :	\
		(core_id == AFT_SS7_CORE_ID) ? "SS7"   :	\
						"Unknown"
#define WAN_ASSERT(val) 						\
	if (val){							\
		log(LOG_INFO, "********** ASSERT FAILED **********\n");	\
		log(LOG_INFO, "%s:%d - Critical error\n",		\
						__FILE__,__LINE__);	\
		return -EINVAL;						\
	}

#define WAN_ASSERT1(val) 						\
	if (val){							\
		log(LOG_INFO, "********** ASSERT FAILED **********\n");	\
		log(LOG_INFO, "%s:%d - Critical error\n",		\
						__FILE__,__LINE__);	\
		return;							\
	}

#define WAN_ASSERT2(val, ret)						\
	if (val){							\
		log(LOG_INFO, "********** ASSERT FAILED **********\n");	\
		log(LOG_INFO, "%s:%d - Critical error\n",		\
						__FILE__,__LINE__);	\
		return ret;						\
	}

#define SDLA_MAGIC(hw)	WAN_ASSERT((hw)->magic != SDLAHW_MAGIC)
/*
******************************************************************
**			S T R U C T U R E S			**	
******************************************************************
*/

typedef struct	sfm_info		/* firmware module information */
{
	unsigned short	codeid;		/* firmware ID */
	unsigned short	version;	/* firmaware version number */
	unsigned short	adapter[SFM_MAX_SDLA]; /* compatible adapter types */
	unsigned long	memsize;	/* minimum memory size */
	unsigned short	reserved[2];	/* reserved */
	unsigned short	startoffs;	/* entry point offset */
	unsigned short	winoffs;	/* dual-port memory window offset */
	unsigned short	codeoffs;	/* code load offset */
	unsigned short	codesize;	/* code size */
	unsigned short	dataoffs;	/* configuration data load offset */
	unsigned short	datasize;	/* configuration data size */
} sfm_info_t;

typedef struct sfm			/* SDLA firmware file structire */
{
	char		signature[80];	/* SFM file signature */
	unsigned short	version;	/* file format version */
	unsigned short	checksum;	/* info + image */
	unsigned short	reserved[6];	/* reserved */
	char		descr[SFM_DESCR_LEN]; /* description string */
	sfm_info_t	info;		/* firmware module info */
	unsigned char	image[1];	/* code image (variable size) */
} sfm_t;


typedef struct sdla_hw_type_cnt
{
	unsigned char AFT_adapters;
}sdla_hw_type_cnt_t;

/****** Function Prototypes *************************************************/
extern int san_dev_attach(void*, u_int8_t*);

/* Hardware interface function */
extern int sdladrv_init(void);
extern int sdladrv_exit(void);
extern int sdla_get_hw_devices(void);
extern void *sdla_get_hw_adptr_cnt(void);

extern int sdla_setup (void*);
extern int sdla_down (void*);
extern int sdla_read_int_stat (void*, u_int32_t*);
extern int sdla_check_mismatch(void*, unsigned char);
extern int sdla_cmd (void*, unsigned long, wan_mbox_t*);
extern int sdla_getcfg(void*, int, void*);
extern int sdla_bus_write_1(void*, unsigned int, u_int8_t);
extern int sdla_bus_write_2(void*, unsigned int, u_int16_t);
extern int sdla_bus_write_4(void*, unsigned int, u_int32_t);
extern int sdla_bus_read_1(void*, unsigned int, u_int8_t*);
extern int sdla_bus_read_2(void*, unsigned int, u_int16_t*);
extern int sdla_bus_read_4(void*, unsigned int, u_int32_t*);
extern int sdla_peek (void*, unsigned long, void*, unsigned);
extern int sdla_poke (void*, unsigned long, void*, unsigned);
extern int sdla_poke_byte (void*, unsigned long, u_int8_t);
extern int sdla_set_bit (void*, unsigned long, u_int8_t);
extern int sdla_clear_bit (void*, unsigned long, u_int8_t);
extern int sdla_intr_establish(void*, int(*intr_func)(void*), void*);
extern int sdla_intr_disestablish(void*);
extern int sdla_get_hwprobe(void*, void**);
extern int sdla_get_hwcard(void*, void**);
extern int sdla_is_te1(void*);

#endif

#undef EXTERN 
#endif	/* __IF_SANDRV_H */

/*
 ****************************************************************************************
 *
 * APP/BOOT  DESCRIPTOR  STUFF
 *
 ****************************************************************************************
 */

/* Define the struct that is initialized by the bootloader used by the 
 * startup code.
 *
 * Copyright (c) 2004, 2005, 2006 Cavium Networks.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <machine/octeon_pcmap_regs.h>
#include <machine/bootinfo.h>

#define OCTEON_CURRENT_DESC_VERSION     6
#define OCTEON_ARGV_MAX_ARGS            (64)
#define OCTOEN_SERIAL_LEN 20

#define MAX_APP_DESC_ADDR     0xffffffffafffffff

typedef struct {
	/* Start of block referenced by assembly code - do not change! */
	uint32_t desc_version;
	uint32_t desc_size;

	uint64_t stack_top;
	uint64_t heap_base;
	uint64_t heap_end;
	uint64_t entry_point;   /* Only used by bootloader */
	uint64_t desc_vaddr;
	/* End of This block referenced by assembly code - do not change! */

	uint32_t exception_base_addr;
	uint32_t stack_size;
	uint32_t heap_size;
	uint32_t argc;  /* Argc count for application */
	uint32_t argv[OCTEON_ARGV_MAX_ARGS];
	uint32_t flags;
	uint32_t core_mask;
	uint32_t dram_size;  /**< DRAM size in megabyes */
	uint32_t phy_mem_desc_addr;  /**< physical address of free memory descriptor block*/
	uint32_t debugger_flags_base_addr;  /**< used to pass flags from app to debugger */
	uint32_t eclock_hz;  /**< CPU clock speed, in hz */
	uint32_t dclock_hz;  /**< DRAM clock speed, in hz */
	uint32_t spi_clock_hz;  /**< SPI4 clock in hz */
	uint16_t board_type;
	uint8_t board_rev_major;
	uint8_t board_rev_minor;
	uint16_t chip_type;
	uint8_t chip_rev_major;
	uint8_t chip_rev_minor;
	char board_serial_number[OCTOEN_SERIAL_LEN];
	uint8_t mac_addr_base[6];
	uint8_t mac_addr_count;
	uint64_t cvmx_desc_vaddr;
} octeon_boot_descriptor_t;


typedef struct {
	uint32_t major_version;
	uint32_t minor_version;

	uint64_t stack_top;
	uint64_t heap_base;
	uint64_t heap_end;
	uint64_t desc_vaddr;

	uint32_t exception_base_addr;
	uint32_t stack_size;
	uint32_t flags;
	uint32_t core_mask;
	uint32_t dram_size;  /**< DRAM size in megabyes */
	uint32_t phy_mem_desc_addr;  /**< physical address of free memory descriptor block*/
	uint32_t debugger_flags_base_addr;  /**< used to pass flags from app to debugger */
	uint32_t eclock_hz;  /**< CPU clock speed, in hz */
	uint32_t dclock_hz;  /**< DRAM clock speed, in hz */
	uint32_t spi_clock_hz;  /**< SPI4 clock in hz */
	uint16_t board_type;
	uint8_t board_rev_major;
	uint8_t board_rev_minor;
	uint16_t chip_type;
	uint8_t chip_rev_major;
	uint8_t chip_rev_minor;
	char board_serial_number[OCTOEN_SERIAL_LEN];
	uint8_t mac_addr_base[6];
	uint8_t mac_addr_count;
} cvmx_bootinfo_t;

uint32_t octeon_cpu_clock;
uint64_t octeon_dram;
uint32_t octeon_bd_ver = 0, octeon_cvmx_bd_ver = 0, octeon_board_rev_major, octeon_board_rev_minor, octeon_board_type;
uint8_t octeon_mac_addr[6] = { 0 };
int octeon_core_mask, octeon_mac_addr_count;
int octeon_chip_rev_major = 0, octeon_chip_rev_minor = 0, octeon_chip_type = 0;

static octeon_boot_descriptor_t *app_desc_ptr;
static cvmx_bootinfo_t *cvmx_desc_ptr;

#define OCTEON_BOARD_TYPE_NONE 			0
#define OCTEON_BOARD_TYPE_SIM  			1
#define	OCTEON_BOARD_TYPE_CN3010_EVB_HS5	11

#define OCTEON_CLOCK_MIN     (100 * 1000 * 1000)
#define OCTEON_CLOCK_MAX     (800 * 1000 * 1000)
#define OCTEON_DRAM_DEFAULT  (256 * 1024 * 1024)
#define OCTEON_DRAM_MIN	     30
#define OCTEON_DRAM_MAX	     3000


int
octeon_board_real(void)
{
	switch (octeon_board_type) {
	case OCTEON_BOARD_TYPE_NONE:
	case OCTEON_BOARD_TYPE_SIM:
		return 0;
	case OCTEON_BOARD_TYPE_CN3010_EVB_HS5:
		/*
		 * XXX
		 * The CAM-0100 identifies itself as type 11, revision 0.0,
		 * despite its being rather real.  Disable the revision check
		 * for type 11.
		 */
		return 1;
	default:
		if (octeon_board_rev_major == 0)
			return 0;
		return 1;
	}
}

static void
octeon_process_app_desc_ver_unknown(void)
{
    	printf(" Unknown Boot-Descriptor: Using Defaults\n");

    	octeon_cpu_clock = OCTEON_CLOCK_DEFAULT;
        octeon_dram = OCTEON_DRAM_DEFAULT;
        octeon_board_rev_major = octeon_board_rev_minor = octeon_board_type = 0;
        octeon_core_mask = 1;
        octeon_chip_type = octeon_chip_rev_major = octeon_chip_rev_minor = 0;
        octeon_mac_addr[0] = 0x00; octeon_mac_addr[1] = 0x0f;
        octeon_mac_addr[2] = 0xb7; octeon_mac_addr[3] = 0x10;
        octeon_mac_addr[4] = 0x09; octeon_mac_addr[5] = 0x06;
        octeon_mac_addr_count = 1;
}

static int
octeon_process_app_desc_ver_6(void)
{
	/* XXX Why is 0x00000000ffffffffULL a bad value?  */
	if (app_desc_ptr->cvmx_desc_vaddr == 0 ||
	    app_desc_ptr->cvmx_desc_vaddr == 0xfffffffful) {
            	printf ("Bad cvmx_desc_ptr %p\n", cvmx_desc_ptr);
                return 1;
	}
    	cvmx_desc_ptr =
	    (cvmx_bootinfo_t *)(__intptr_t)app_desc_ptr->cvmx_desc_vaddr;
        cvmx_desc_ptr =
	    (cvmx_bootinfo_t *) ((__intptr_t)cvmx_desc_ptr | CKSEG0_BASE);
        octeon_cvmx_bd_ver = (cvmx_desc_ptr->major_version * 100) +
	    cvmx_desc_ptr->minor_version;
        if (cvmx_desc_ptr->major_version != 1) {
            	panic("Incompatible CVMX descriptor from bootloader: %d.%d %p\n",
                       (int) cvmx_desc_ptr->major_version,
                       (int) cvmx_desc_ptr->minor_version, cvmx_desc_ptr);
        }

        octeon_core_mask = cvmx_desc_ptr->core_mask;
        octeon_cpu_clock  = cvmx_desc_ptr->eclock_hz;
        octeon_board_type = cvmx_desc_ptr->board_type;
        octeon_board_rev_major = cvmx_desc_ptr->board_rev_major;
        octeon_board_rev_minor = cvmx_desc_ptr->board_rev_minor;
        octeon_chip_type = cvmx_desc_ptr->chip_type;
        octeon_chip_rev_major = cvmx_desc_ptr->chip_rev_major;
        octeon_chip_rev_minor = cvmx_desc_ptr->chip_rev_minor;
        octeon_mac_addr[0] = cvmx_desc_ptr->mac_addr_base[0];
        octeon_mac_addr[1] = cvmx_desc_ptr->mac_addr_base[1];
        octeon_mac_addr[2] = cvmx_desc_ptr->mac_addr_base[2];
        octeon_mac_addr[3] = cvmx_desc_ptr->mac_addr_base[3];
        octeon_mac_addr[4] = cvmx_desc_ptr->mac_addr_base[4];
        octeon_mac_addr[5] = cvmx_desc_ptr->mac_addr_base[5];
        octeon_mac_addr_count = cvmx_desc_ptr->mac_addr_count;

        if (app_desc_ptr->dram_size > 16*1024*1024)
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size;
	else
            	octeon_dram = (uint64_t)app_desc_ptr->dram_size << 20;
        return 0;
}

void
octeon_boot_params_init(register_t ptr)
{
	int bad_desc = 1;
	
    	if (ptr != 0 && ptr < MAX_APP_DESC_ADDR) {
	        app_desc_ptr = (octeon_boot_descriptor_t *)(__intptr_t)ptr;
		octeon_bd_ver = app_desc_ptr->desc_version;
		if (app_desc_ptr->desc_version < 6)
			panic("Your boot code is too old to be supported.\n");
		if (app_desc_ptr->desc_version >= 6)
			bad_desc = octeon_process_app_desc_ver_6();
        }
        if (bad_desc)
        	octeon_process_app_desc_ver_unknown();

        printf("Boot Descriptor Ver: %u -> %u/%u",
               octeon_bd_ver, octeon_cvmx_bd_ver/100, octeon_cvmx_bd_ver%100);
        printf("  CPU clock: %uMHz  Core Mask: %#x\n", octeon_cpu_clock/1000000, octeon_core_mask);
        printf("  Dram: %u MB", (uint32_t)(octeon_dram >> 20));
        printf("  Board Type: %u  Revision: %u/%u\n",
               octeon_board_type, octeon_board_rev_major, octeon_board_rev_minor);
        printf("  Octeon Chip: %u  Rev %u/%u",
               octeon_chip_type, octeon_chip_rev_major, octeon_chip_rev_minor);

        printf("  Mac Address %02X.%02X.%02X.%02X.%02X.%02X (%d)\n",
	    octeon_mac_addr[0], octeon_mac_addr[1], octeon_mac_addr[2],
	    octeon_mac_addr[3], octeon_mac_addr[4], octeon_mac_addr[5],
	    octeon_mac_addr_count);
}

unsigned long
octeon_get_clock_rate(void)
{
	return octeon_cpu_clock;
}

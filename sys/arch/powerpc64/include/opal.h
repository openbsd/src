/*	$OpenBSD: opal.h,v 1.5 2020/06/08 18:35:10 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_OPAL_H_
#define _MACHINE_OPAL_H_

/* Tokens. */
#define OPAL_TEST			0
#define OPAL_CONSOLE_WRITE		1
#define OPAL_CONSOLE_READ		2
#define OPAL_CEC_POWER_DOWN		5
#define OPAL_CEC_REBOOT			6
#define OPAL_POLL_EVENTS		10
#define OPAL_PCI_CONFIG_READ_WORD	15
#define OPAL_PCI_CONFIG_WRITE_WORD	18
#define OPAL_PCI_EEH_FREEZE_STATUS	23
#define OPAL_PCI_EEH_FREEZE_CLEAR	26
#define OPAL_PCI_PHB_MMIO_ENABLE	27
#define OPAL_PCI_SET_PHB_MEM_WINDOW	28
#define OPAL_PCI_MAP_PE_MMIO_WINDOW	29
#define OPAL_PCI_SET_PE			31
#define OPAL_PCI_MAP_PE_DMA_WINDOW_REAL	45
#define OPAL_PCI_RESET			49

/* Return codes. */
#define OPAL_SUCCESS			0
#define OPAL_PARAMETER			-1
#define OPAL_BUSY			-2
#define OPAL_PARTIAL			-3
#define OPAL_CONSTRAINED		-4
#define OPAL_CLOSED			-5
#define OPAL_HARDWARE			-6
#define OPAL_UNSUPPORTED		-7
#define OPAL_PERMISSION			-8
#define OPAL_NO_MEM			-9
#define OPAL_RESOURCE			-10
#define OPAL_INTERNAL_ERROR		-11
#define OPAL_BUSY_EVENT			-12
#define OPAL_HARDWARE_FROZEN		-13
#define OPAL_WRONG_STATE		-14
#define OPAL_ASYNC_COMPLETION		-15

/* OPAL_PCI_EEH_FREEZE_CLEAR */
#define OPAL_EEH_ACTION_CLEAR_FREEZE_MMIO 1
#define OPAL_EEH_ACTION_CLEAR_FREEZE_DMA 2
#define OPAL_EEH_ACTION_CLEAR_FREEZE_ALL 3

/* OPAL_PCI_PHB_MMIO_ENABLE */
#define OPAL_M32_WINDOW_TYPE		1
#define OPAL_M64_WINDOW_TYPE		2
#define OPAL_IO_WINDOW_TYPE		3
#define OPAL_DISABLE_M64		0
#define OPAL_ENABLE_M64_SPLIT		1
#define OPAL_ENABLE_M64_NON_SPLIT	2

/* OPAL_PCIE_SET_PE */
#define OPAL_IGNORE_RID_BUS_NUMBER	0
#define OPAL_IGNORE_RID_DEVICE_NUMBER	0
#define OPAL_COMPARE_RID_DEVICE_NUMBER	1
#define OPAL_IGNORE_RID_FUNCTION_NUMBER	0
#define OPAL_COMPARE_RID_FUNCTION_NUMBER 1
#define OPAL_UNMAP_PE			0
#define OPAL_MAP_PE			1

/* OPAL_PCI_RESET */
#define OPAL_RESET_PHB_COMPLETE		1
#define OPAL_RESET_PCI_LINK		2
#define OPAL_RESET_PHB_ERROR		3
#define OPAL_RESET_PCI_HOT		4
#define OPAL_RESET_PCI_FUNDAMENTAL	5
#define OPAL_RESET_PCI_IODA_TABLE	6
#define OPAL_DEASSERT_RESET		0
#define OPAL_ASSERT_RESET		1

#ifndef _LOCORE
int64_t	opal_test(uint64_t);
int64_t	opal_console_write(int64_t, int64_t *, const uint8_t *);
int64_t	opal_console_read(int64_t, int64_t *, uint8_t *);
int64_t	opal_cec_power_down(uint64_t);
int64_t	opal_cec_reboot(void);
int64_t	opal_poll_events(uint64_t *);
int64_t opal_pci_config_read_word(uint64_t, uint64_t, uint64_t, uint32_t *);
int64_t opal_pci_config_write_word(uint64_t, uint64_t, uint64_t, uint32_t);
int64_t opal_pci_eeh_freeze_status(uint64_t, uint64_t, uint8_t *,
	    uint16_t *, uint64_t *);
int64_t opal_pci_eeh_freeze_clear(uint64_t, uint64_t, uint64_t);
int64_t opal_pci_phb_mmio_enable(uint64_t, uint16_t, uint16_t, uint16_t);
int64_t opal_pci_set_phb_mem_window(uint64_t, uint16_t, uint16_t,
	    uint64_t, uint64_t, uint64_t);
int64_t opal_pci_map_pe_mmio_window(uint64_t, uint64_t, uint16_t,
	    uint16_t, uint16_t);
int64_t opal_pci_set_pe(uint64_t, uint64_t, uint64_t, uint8_t, uint8_t,
	    uint8_t, uint8_t);
int64_t opal_pci_map_pe_dma_window_real(uint64_t, uint64_t, uint16_t,
	    uint64_t, uint64_t);
int64_t opal_pci_reset(uint64_t, uint8_t, uint8_t);

void	opal_printf(const char *fmt, ...);
#endif

#endif /* _MACHINE_OPAL_H_ */

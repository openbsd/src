/*	$OpenBSD: ixgbe.h,v 1.1 2008/06/08 20:01:02 reyk Exp $	*/

/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: src/sys/dev/ixgbe/ixgbe_osdep.h,v 1.4 2008/05/16 18:46:30 jfv Exp $*/

#ifndef _IXGBE_OS_H_
#define _IXGBE_OS_H_

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/workq.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/bpf.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/rndvar.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t	 s32;
typedef uint64_t u64;
typedef boolean_t bool;

#include <dev/pci/ixgbe_type.h>

#define ASSERT(x) if(!(x)) panic("IXGBE: x")

/* The happy-fun DELAY macro is defined in /usr/src/sys/i386/include/clock.h */
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))

#define DBG 0 
#define MSGOUT(S, A, B)     printf(S "\n", A, B)
#define DEBUGFUNC(F)        DEBUGOUT(F);
#if DBG
#define DEBUGOUT(S)         printf(S "\n")
#define DEBUGOUT1(S,A)      printf(S "\n",A)
#define DEBUGOUT2(S,A,B)    printf(S "\n",A,B)
#define DEBUGOUT3(S,A,B,C)  printf(S "\n",A,B,C)
#define DEBUGOUT7(S,A,B,C,D,E,F,G)  printf(S "\n",A,B,C,D,E,F,G)
#else
#define DEBUGOUT(S)
#define DEBUGOUT1(S,A)
#define DEBUGOUT2(S,A,B)
#define DEBUGOUT3(S,A,B,C)
#define DEBUGOUT6(S,A,B,C,D,E,F)
#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define FALSE               0
#define TRUE                1
#define CMD_MEM_WRT_INVALIDATE          0x0010  /* BIT_4 */
#define PCI_COMMAND_REGISTER            PCIR_COMMAND

typedef struct device device_t;
#define TUNABLE_INT(x...)
#define DRIVER_MODULE(x...)
#define MODULE_DEPEND(x...)

/* This is needed by the shared code */
struct ixgbe_hw; 
extern u16 ixgbe_read_pci_cfg(struct ixgbe_hw *, u32);
#define IXGBE_READ_PCIE_WORD ixgbe_read_pci_cfg

struct ixgbe_osdep {
	bus_dma_tag_t		 os_dmat;
	bus_space_tag_t		 os_memt;
	bus_space_handle_t	 os_memh;

	bus_size_t		 os_memsize;
	bus_addr_t		 os_membase;

	void			*os_sc;
	struct pci_attach_args	*os_pa;
};

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

#define IXGBE_READ_REG(a, reg)						\
	bus_space_read_4(((struct ixgbe_osdep *)(a)->back)->os_memt,	\
	((struct ixgbe_osdep *)(a)->back)->os_memh, reg)
#define IXGBE_WRITE_REG(a, reg, value)					\
	bus_space_write_4(((struct ixgbe_osdep *)(a)->back)->os_memt,	\
	((struct ixgbe_osdep *)(a)->back)->os_memh, reg, value)
#define IXGBE_READ_REG_ARRAY(a, reg, offset)				\
	bus_space_read_4(((struct ixgbe_osdep *)(a)->back)->os_memt,	\
	((struct ixgbe_osdep *)(a)->back)->os_memh, (reg + ((offset) << 2))))
#define IXGBE_WRITE_REG_ARRAY(a, reg, offset, value)			\
	bus_space_write_4(((struct ixgbe_osdep *)(a)->back)->os_memt,	\
	((struct ixgbe_osdep *)(a)->back)->os_memh, (reg + ((offset) << 2)), value)

/* FreeBSD compat glue */
#define MJUMPAGESIZE	MCLBYTES
#define PCIR_BAR(_x)	(0x10 + (_x) * 4)
#define roundup2(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))

s32 ixgbe_init_ops_generic(struct ixgbe_hw *hw);
s32 ixgbe_init_hw_generic(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_generic(struct ixgbe_hw *hw);
s32 ixgbe_clear_hw_cntrs_generic(struct ixgbe_hw *hw);
s32 ixgbe_read_pba_num_generic(struct ixgbe_hw *hw, u32 *pba_num);
s32 ixgbe_get_mac_addr_generic(struct ixgbe_hw *hw, u8 *mac_addr);
s32 ixgbe_get_bus_info_generic(struct ixgbe_hw *hw);
s32 ixgbe_stop_adapter_generic(struct ixgbe_hw *hw);

s32 ixgbe_led_on_generic(struct ixgbe_hw *hw, u32 index);
s32 ixgbe_led_off_generic(struct ixgbe_hw *hw, u32 index);

s32 ixgbe_init_eeprom_params_generic(struct ixgbe_hw *hw);
s32 ixgbe_write_eeprom_generic(struct ixgbe_hw *hw, u16 offset, u16 data);
s32 ixgbe_read_eeprom_generic(struct ixgbe_hw *hw, u16 offset, u16 *data);
s32 ixgbe_read_eeprom_bit_bang_generic(struct ixgbe_hw *hw, u16 offset,
                                       u16 *data);
s32 ixgbe_validate_eeprom_checksum_generic(struct ixgbe_hw *hw,
                                           u16 *checksum_val);
s32 ixgbe_update_eeprom_checksum_generic(struct ixgbe_hw *hw);

s32 ixgbe_set_rar_generic(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
                          u32 enable_addr);
s32 ixgbe_init_rx_addrs_generic(struct ixgbe_hw *hw);
s32 ixgbe_update_mc_addr_list_generic(struct ixgbe_hw *hw, u8 *mc_addr_list,
                                      u32 mc_addr_count,
                                      ixgbe_mc_addr_itr func);
s32 ixgbe_update_uc_addr_list_generic(struct ixgbe_hw *hw, u8 *addr_list,
                                      u32 addr_count, ixgbe_mc_addr_itr func);
s32 ixgbe_enable_mc_generic(struct ixgbe_hw *hw);
s32 ixgbe_disable_mc_generic(struct ixgbe_hw *hw);
s32 ixgbe_clear_vfta_generic(struct ixgbe_hw *hw);
s32 ixgbe_set_vfta_generic(struct ixgbe_hw *hw, u32 vlan,
                           u32 vind, bool vlan_on);

s32 ixgbe_validate_mac_addr(u8 *mac_addr);
s32 ixgbe_acquire_swfw_sync(struct ixgbe_hw *hw, u16 mask);
void ixgbe_release_swfw_sync(struct ixgbe_hw *hw, u16 mask);
s32 ixgbe_disable_pcie_master(struct ixgbe_hw *hw);

s32 ixgbe_read_analog_reg8_generic(struct ixgbe_hw *hw, u32 reg, u8 *val);
s32 ixgbe_write_analog_reg8_generic(struct ixgbe_hw *hw, u32 reg, u8 val);

void ixgbe_set_mta(struct ixgbe_hw *hw, u8 *mc_addr);
s32 ixgbe_init_ops_82598(struct ixgbe_hw *hw);

/* PHY */
s32 ixgbe_init_phy_ops_generic(struct ixgbe_hw *hw);
bool ixgbe_validate_phy_addr(struct ixgbe_hw *hw, u32 phy_addr);
enum ixgbe_phy_type ixgbe_get_phy_type_from_id(u32 phy_id);
s32 ixgbe_get_phy_id(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_generic(struct ixgbe_hw *hw);
s32 ixgbe_reset_phy_generic(struct ixgbe_hw *hw);
s32 ixgbe_read_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
                               u32 device_type, u16 *phy_data);
s32 ixgbe_write_phy_reg_generic(struct ixgbe_hw *hw, u32 reg_addr,
                                u32 device_type, u16 phy_data);
s32 ixgbe_setup_phy_link_generic(struct ixgbe_hw *hw);
s32 ixgbe_setup_phy_link_speed_generic(struct ixgbe_hw *hw,
                                       ixgbe_link_speed speed,
                                       bool autoneg,
                                       bool autoneg_wait_to_complete);

s32 ixgbe_check_phy_link_tnx(struct ixgbe_hw *hw,
                             ixgbe_link_speed *speed,
                             bool *link_up);
s32 ixgbe_get_phy_firmware_version_tnx(struct ixgbe_hw *hw,
                                       u16 *firmware_version);

s32 ixgbe_reset_phy_nl(struct ixgbe_hw *hw);

#endif /* _IXGBE_OS_H_ */

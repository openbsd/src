/**************************************************************************

Copyright (c) 2001-2002 Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms of the Software, with or
without modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code of the Software may retain the above
    copyright notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form of the Software may reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors shall be used to endorse or promote products derived from
    this Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

***************************************************************************/

/*$FreeBSD$*/

#ifndef _OPENBSD_OS_H_
#define _OPENBSD_OS_H_

#define ASSERT(x) if(!(x)) panic("EM: x")

/* The happy-fun DELAY macro is defined in /usr/src/sys/i386/include/clock.h */
#define usec_delay(x) DELAY(x)
#define msec_delay(x) DELAY(1000*(x))

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
	#define DEBUGOUT7(S,A,B,C,D,E,F,G)
#endif

#define FALSE               0
#define TRUE                1
#define CMD_MEM_WRT_INVALIDATE          0x0010  /* BIT_4 */
#define PCI_COMMAND_REGISTER            PCI_COMMAND_STATUS_REG 

struct em_dmamap
{
        bus_size_t              emm_size;
        caddr_t                 emm_ptr, emm_kva;
        bus_dma_segment_t       emm_seg;
        bus_dmamap_t            emm_dmamap;
        int                     emm_rseg;
};

struct em_osdep
{
	struct device     *dev;

	struct em_dmamap   em_rx;
	struct em_dmamap   em_tx;

	struct pci_attach_args em_pa;

	bus_space_handle_t      em_bhandle;
        bus_space_tag_t         em_btag;
        bus_size_t              em_memsize;
        bus_addr_t              em_membase;

        bus_space_handle_t      em_iobhandle;
        bus_space_tag_t         em_iobtag;
        bus_size_t              em_iosize;
        bus_addr_t              em_iobase;

};

#define E1000_READ_REG(hw, reg) \
   bus_space_read_4( \
		((struct em_osdep *)(hw)->back)->em_btag, \
		((struct em_osdep *)(hw)->back)->em_bhandle, \
		((hw)->mac_type >= em_82543) ? \
			E1000_##reg : E1000_82542_##reg)

#define E1000_WRITE_REG(hw, reg, value) \
   bus_space_write_4( \
		((struct em_osdep *)(hw)->back)->em_btag, \
		((struct em_osdep *)(hw)->back)->em_bhandle, \
		((hw)->mac_type >= em_82543) ? \
                     	E1000_##reg : E1000_82542_##reg, \
		value)

#define E1000_READ_REG_ARRAY(sc, reg, offset) \
   bus_space_read_4( \
		((struct em_osdep *)(hw)->back)->em_btag, \
		((struct em_osdep *)(hw)->back)->em_bhandle, \
		((hw)->mac_type >= em_82543) ? \
			(E1000_##reg       + ((offset) << 2)) : \
			(E1000_82542_##reg + ((offset) << 2)) )

#define E1000_WRITE_REG_ARRAY(sc, reg, offset, value) \
      bus_space_write_4( \
		((struct em_osdep *)(hw)->back)->em_btag, \
		((struct em_osdep *)(hw)->back)->em_bhandle, \
		((hw)->mac_type >= em_82543) ? \
			(E1000_##reg       + ((offset) << 2)) : \
			(E1000_82542_##reg + ((offset) << 2)), \
		value)

#endif  /* _OPENBSD_OS_H_ */


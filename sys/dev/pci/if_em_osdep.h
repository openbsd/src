/**************************************************************************

Copyright (c) 2001-2003, Intel Corporation
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

***************************************************************************/

/*$FreeBSD: if_em_osdep.h,v 1.11 2003/05/02 21:17:08 pdeuskar Exp $*/
/* $OpenBSD: if_em_osdep.h,v 1.3 2004/04/18 04:15:01 henric Exp $ */

#ifndef _EM_OPENBSD_OS_H_
#define _EM_OPENBSD_OS_H_

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

struct em_osdep
{
	bus_space_tag_t    mem_bus_space_tag;
	bus_space_handle_t mem_bus_space_handle;
	struct device     *dev;

	struct pci_attach_args em_pa;

        bus_size_t              em_memsize;
        bus_addr_t              em_membase;

        bus_space_handle_t      em_iobhandle;
        bus_space_tag_t         em_iobtag;
        bus_size_t              em_iosize;
        bus_addr_t              em_iobase;
};

#define E1000_WRITE_FLUSH(a) E1000_READ_REG(a, STATUS)

#define E1000_READ_REG(a, reg) 						\
   bus_space_read_4( ((struct em_osdep *)(a)->back)->mem_bus_space_tag,	\
	((struct em_osdep *)(a)->back)->mem_bus_space_handle,		\
	((a)->mac_type >= em_82543) ? E1000_##reg : E1000_82542_##reg)

#define E1000_WRITE_REG(a, reg, value)					\
   bus_space_write_4( ((struct em_osdep *)(a)->back)->mem_bus_space_tag, \
	((struct em_osdep *)(a)->back)->mem_bus_space_handle,		\
	((a)->mac_type >= em_82543) ? E1000_##reg : E1000_82542_##reg,	\
	value)

#define E1000_READ_REG_ARRAY(a, reg, offset)				\
   bus_space_read_4( ((struct em_osdep *)(a)->back)->mem_bus_space_tag,	\
		     ((struct em_osdep *)(a)->back)->mem_bus_space_handle, \
		     ((a)->mac_type >= em_82543) ?			\
				(E1000_##reg	   + ((offset) << 2)) :	\
				(E1000_82542_##reg + ((offset) << 2)) ) 

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value)			\
    bus_space_write_4( ((struct em_osdep *)(a)->back)->mem_bus_space_tag, \
	((struct em_osdep *)(a)->back)->mem_bus_space_handle,		\
	((a)->mac_type >= em_82543) ?					\
		(E1000_##reg	   + ((offset) << 2)) :			\
		(E1000_82542_##reg + ((offset) << 2)),			\
	value)

#define em_io_read(hw, port)						\
        bus_space_read_4(((struct em_osdep *)(hw)->back)->em_iobtag,	\
                ((struct em_osdep *)(hw)->back)->em_iobhandle, (port))

#define em_io_write(hw, port, value)					\
        bus_space_write_4(((struct em_osdep *)(hw)->back)->em_iobtag,	\
                        ((struct em_osdep *)(hw)->back)->em_iobhandle,	\
			(port), (value))

#ifdef DEBUG
#define EM_KASSERT(exp,msg)        do { if (!(exp)) panic msg; } while (0)
#else
#define EM_KASSERT(exp,msg)
#endif
#define bus_dma_tag_destroy(tag)
#define mtx_assert(a, b)        splassert(IPL_NET)

#endif  /* _EM_OPENBSD_OS_H_ */


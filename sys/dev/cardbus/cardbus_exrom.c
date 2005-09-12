/*	$OpenBSD: cardbus_exrom.c,v 1.3 2005/09/12 22:52:50 miod Exp $	*/
/*	$NetBSD: cardbus_exrom.c,v 1.4 2000/02/03 06:47:31 thorpej Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to
 * The NetBSD Foundation by Johan Danielsson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/cardbus/cardbus_exrom.h>

#if defined(CARDBUS_DEBUG)
#define	DPRINTF(a)	printf a
#else
#define	DPRINTF(a)
#endif

#define READ_INT16(T, H, O)  \
	(bus_space_read_1((T), (H), (O)) | \
	 (bus_space_read_1((T), (H), (O) + 1) << 8))

/*
 *  A PCI ROM is divided into a number of images. Each image has two
 *  data structures, a header located at the start of the image, and a
 *  `data structure' at some offset into it.
 *
 *  The header is a 26 byte structure:
 *
 *  Offset	Length	Description
 *  0x00	   1	signature byte 1 (0x55)
 *  0x01	   1	signature byte 2 (0xAA)
 *  0x02	  22	processor architecture data
 *  0x18	   2	pointer to the data structure
 *
 *  The data structure is a 24 byte structure:
 *
 *  Offset	Length	Description
 *  0x00	   4	signature (PCIR)
 *  0x04	   2	vendor id
 *  0x06	   2	device id
 *  0x08	   2	reserved
 *  0x0A	   2	data structure length
 *  0x0C	   1	data structure revision (0)
 *  0x0D	   3	class code
 *  0x10	   2	image length (in 512 byte blocks)
 *  0x12	   2	code revision level
 *  0x14	   1	code type
 *  0x15	   1	indicator (bit 7 indicates final image)
 *  0x16	   2	reserved
 *
 */

/*
 *  Scan through a PCI expansion ROM, and create subregions for each
 *  ROM image. This function assumes that the ROM is mapped at
 *  (tag,handle), and that the expansion ROM address decoder is
 *  enabled. The PCI specification requires that no other BAR should
 *  be accessed while the ROM is enabled, so interrupts should be
 *  disabled.
 *
 * XXX This routine is way too pessimistic and returns as soon as it encounters
 * a problem, although not being able to malloc or read a particular image
 * may not prevent further images from being read successfully.
 */
int
cardbus_read_exrom(bus_space_tag_t romt, bus_space_handle_t romh,
    struct cardbus_rom_image_head *head)
{
	size_t addr = 0; /* offset of current rom image */
	size_t dataptr;
	unsigned int rom_image = 0;
	size_t image_size;
	struct cardbus_rom_image *image;
	u_int16_t val;
    
	SIMPLEQ_INIT(head);
	do {
		val = READ_INT16(romt, romh, addr + CARDBUS_EXROM_SIGNATURE);
		if (val != 0xaa55) {
			DPRINTF(("%s: bad header signature in ROM image %u: 0x%04x\n",
			    __func__, rom_image, val));
			return (1);
		}
		dataptr = addr +
		    READ_INT16(romt, romh, addr + CARDBUS_EXROM_DATA_PTR);

		/* get the ROM image size, in blocks */
		image_size = READ_INT16(romt, romh, 
		    dataptr + CARDBUS_EXROM_DATA_IMAGE_LENGTH);
		/* XXX
		 * Some ROMs seem to have this as zero, can we assume
		 * this means 1 block?
		 */
		if (image_size == 0) 
			image_size = 1;
		image_size <<= 9;

		image = malloc(sizeof(*image), M_DEVBUF, M_NOWAIT);
		if (image == NULL) {
			DPRINTF(("%s: out of memory\n", __func__));
			return (1);
		}
		image->rom_image = rom_image;
		image->image_size = image_size;
		image->romt = romt;
		if (bus_space_subregion(romt, romh, addr,
		    image_size, &image->romh)) {
			DPRINTF(("%s: bus_space_subregion failed", __func__));
			free(image, M_DEVBUF);
			return (1);
		}
		SIMPLEQ_INSERT_TAIL(head, image, next);
		addr += image_size;
		rom_image++;
	} while ((bus_space_read_1(romt, romh,
	    dataptr + CARDBUS_EXROM_DATA_INDICATOR) & 0x80) == 0);
	return (0);
}

#if 0
struct cardbus_exrom_data_structure {
	char		signature[4];
	cardbusreg_t	id;		/* vendor & device id */
	u_int16_t	structure_length;
	u_int8_t	structure_revision;
	cardbusreg_t	class;		/* class code in upper 24 bits */
	u_int16_t	image_length;
	u_int16_t	data_revision;
	u_int8_t	code_type;
	u_int8_t	indicator;
};

int
pci_exrom_parse_data_structure(bus_space_tag_t tag,
    bus_space_handle_t handle, struct pci_exrom_data_structure *ds)
{
	unsigned char hdr[16];
	int length;

	bus_space_read_region_1(tag, handle, dataptr, hdr, sizeof(hdr));
	memcpy(header->signature, hdr + PCI_EXROM_DATA_SIGNATURE, 4);
#define LEINT16(B, O)	((B)[(O)] | ((B)[(O) + 1] << 8))
	header->id = LEINT16(hdr, PCI_EXROM_DATA_VENDOR_ID) |
	    (LEINT16(hdr, PCI_EXROM_DATA_DEVICE_ID) << 16);
	header->structure_length = LEINT16(hdr, PCI_EXROM_DATA_LENGTH);
	header->structure_rev = hdr[PCI_EXROM_DATA_REV];
	header->class = (hdr[PCI_EXROM_DATA_CLASS_CODE] << 8) |
	    (hdr[PCI_EXROM_DATA_CLASS_CODE + 1] << 16) |
	    (hdr[PCI_EXROM_DATA_CLASS_CODE + 2] << 24);
	header->image_length = LEINT16(hdr, PCI_EXROM_DATA_IMAGE_LENGTH) << 16;
	header->data_revision = LEINT16(hdr, PCI_EXROM_DATA_DATA_REV);
	header->code_type = hdr[PCI_EXROM_DATA_CODE_TYPE];
	header->indicator = hdr[PCI_EXROM_DATA_INDICATOR];

	length = min(length, header->image_length - 0x18 - offset);
	bus_space_read_region_1(tag, handle, dataptr + 0x18 + offset,
	    buf, length);

	return (length);
}
#endif

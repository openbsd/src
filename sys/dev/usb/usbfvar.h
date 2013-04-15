/*	$OpenBSD: usbfvar.h,v 1.8 2013/04/15 09:23:02 mglocker Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

/*
 * USB function driver interface
 *
 * This file is to be included only by the logical device driver and the
 * USB device controller (DC) driver.
 */

/*** structures corresponding to USB protocol components ***/

struct usbf_endpoint {
	struct usbf_interface	    *iface;
	usb_endpoint_descriptor_t   *edesc;
	int			     halted;	/* UF_ENDPOINT_HALT set */
	int			     refcnt;
	SIMPLEQ_ENTRY(usbf_endpoint) next;
};

struct usbf_interface {
	struct usbf_config	     *config;
	usb_interface_descriptor_t   *idesc;
	LIST_HEAD(, usbf_pipe)        pipes;
	SIMPLEQ_HEAD(, usbf_endpoint) endpoint_head;
	SIMPLEQ_ENTRY(usbf_interface) next;
};

struct usbf_config {
	struct usbf_device *uc_device;
	usb_config_descriptor_t *uc_cdesc;
	size_t uc_cdesc_size;
	int uc_closed;
	SIMPLEQ_HEAD(, usbf_interface) iface_head;
	SIMPLEQ_ENTRY(usbf_config) next;
};

struct usbf_device {
	struct device		 bdev;		/* base device */
	struct usbf_bus		*bus;		/* device controller */
	struct usbf_function	*function;	/* function driver */
	struct usbf_pipe	*default_pipe;	/* pipe 0 (device control) */
	struct usbf_xfer	*default_xfer;	/* device request xfer */
	struct usbf_xfer	*data_xfer;	/* request response xfer */
	int			 address;	/* assigned by host (or 0) */
	struct usbf_config	*config;	/* set by host (or NULL) */
	usb_status_t		 status;	/* device status */
	usb_device_request_t	 def_req;	/* device request buffer */
	struct usbf_endpoint	 def_ep;	/* for pipe 0 */
	usb_endpoint_descriptor_t def_ep_desc;	/* for pipe 0 */
	usb_device_descriptor_t  ddesc;		/* device descriptor */
	usb_string_descriptor_t *sdesc;		/* string descriptors */
	size_t			 sdesc_size;	/* size of ud_sdesc */
	uByte			 string_id;	/* next string id */
	SIMPLEQ_HEAD(, usbf_config) configs;
};

/*** software control structures ***/

struct usbf_pipe_methods {
	usbf_status	(*transfer)(struct usbf_xfer *);
	usbf_status	(*start)(struct usbf_xfer *);
	void		(*abort)(struct usbf_xfer *);
	void		(*done)(struct usbf_xfer *);
	void		(*close)(struct usbf_pipe *);
};

struct usbf_bus_methods {
	usbf_status	  (*open_pipe)(struct usbf_pipe *);
	void		  (*soft_intr)(void *);
	usbf_status	  (*allocm)(struct usbf_bus *, struct usb_dma *,
			      u_int32_t);
	void		  (*freem)(struct usbf_bus *, struct usb_dma *);
	struct usbf_xfer *(*allocx)(struct usbf_bus *);
	void		  (*freex)(struct usbf_bus *, struct usbf_xfer *);
};

struct usbf_softc;

struct usbf_bus {
	/* Filled by DC driver */
	struct device		 bdev;		/* base device */
	struct usbf_bus_methods	*methods;
	size_t			 pipe_size;	/* size of pipe struct */
	u_int8_t		 ep0_maxp;	/* packet size for EP0 */
	int			 usbrev;	/* as in struct usbd_bus */
	/* Filled by usbf driver */
	struct usbf_softc	*usbfctl;
	int			 intr_context;
	void			*soft;		/* soft interrupt cookie */
	bus_dma_tag_t		 dmatag;	/* DMA tag */
};

struct usbf_port {
	usb_port_status_t	 status;
	u_int8_t		 portno;
	struct usbf_device	*device;	/* connected function */
};

struct usbf_pipe {
	struct usbf_device	 *device;
	struct usbf_interface	 *iface;	/* unless default pipe */
	struct usbf_endpoint	 *endpoint;
	int			  refcnt;
	int			  running;
	int			  aborting;
	SIMPLEQ_HEAD(, usbf_xfer) queue;
	LIST_ENTRY(usbf_pipe)	  next;

	char			  repeat;
	int			  interval;

	/* Filled by DC driver. */
	struct usbf_pipe_methods *methods;
};

struct usbf_xfer {
	struct usbf_pipe	*pipe;
	void	 		*priv;
	void			*buffer;
	u_int32_t		 length;
	u_int32_t		 actlen;
	u_int16_t		 flags;
	u_int32_t		 timeout;
	usbf_status		 status;
	usbf_callback		 callback;
	SIMPLEQ_ENTRY(usbf_xfer) next;

	/* for memory management */
	struct usbf_device	*device;
	int			 rqflags;
	struct usb_dma		 dmabuf;

	struct timeout		 timeout_handle;
};


/* usbf.c */
void	    usbf_host_reset(struct usbf_bus *);
void	    usbf_do_request(struct usbf_xfer *, void *, usbf_status);

/* usbf_subr.c */
usbf_status usbf_new_device(struct device *, struct usbf_bus *, int, int, int,
				     struct usbf_port *);
usbf_status usbf_set_endpoint_feature(struct usbf_config *, u_int8_t,
				      u_int16_t);
usbf_status usbf_clear_endpoint_feature(struct usbf_config *, u_int8_t,
					u_int16_t);
usbf_status usbf_insert_transfer(struct usbf_xfer *xfer);
void	    usbf_transfer_complete(struct usbf_xfer *xfer);
usbf_status usbf_allocmem(struct usbf_bus *, size_t, size_t, struct usb_dma *);
void	    usbf_freemem(struct usbf_bus *, struct usb_dma *);
usbf_status usbf_softintr_establish(struct usbf_bus *);
void	    usbf_schedsoftintr(struct usbf_bus *);

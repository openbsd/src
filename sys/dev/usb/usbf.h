/*	$OpenBSD: usbf.h,v 1.4 2013/04/15 09:23:02 mglocker Exp $	*/

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
 * USB function driver interface data structures and subroutines
 */

#ifndef _USBF_H_
#define _USBF_H_

struct usbf_function;
struct usbf_bus;
struct usbf_device;
struct usbf_config;
struct usbf_interface;
struct usbf_endpoint;
struct usbf_pipe;
struct usbf_xfer;

/*
 * Return codes for many of the function driver interface routines
 */
typedef enum { /* keep in sync with USBF_ERROR_STRS */
	USBF_NORMAL_COMPLETION = 0,	/* must be 0 */
	USBF_IN_PROGRESS,		/* 1 */
	/* errors */
	USBF_NOT_STARTED,		/* 2 */
	USBF_INVAL,			/* 3 */
	USBF_NOMEM,			/* 4 */
	USBF_CANCELLED,			/* 5 */
	USBF_BAD_ADDRESS,		/* 6 */
	USBF_IOERROR,			/* 7 */
	USBF_TIMEOUT,			/* 8 */
	USBF_SHORT_XFER,		/* 9 */
	USBF_STALLED,			/* 10 */
	USBF_ERROR_MAX			/* must be last */
} usbf_status;
#define USBF_ERROR_STRS { /* keep in sync with enum usbf_status */	\
	"NORMAL_COMPLETION",		/* 0 */				\
	"IN_PROGRESS",			/* 1 */				\
	"NOT_STARTED",			/* 2 */				\
	"INVAL",			/* 3 */				\
	"NOMEM",			/* 4 */				\
	"CANCELLED",			/* 5 */				\
	"BAD_ADDRESS",			/* 6 */				\
	"IOERROR",			/* 7 */				\
	"TIMEOUT",			/* 8 */				\
	"SHORT_XFER",			/* 9 */				\
	"STALLED",			/* 10 */			\
};

typedef void (*usbf_callback)(struct usbf_xfer *, void *, usbf_status);

/*
 * Attach USB function at the logical device.
 */
struct usbf_attach_arg {
	struct usbf_device *device;
};

struct usbf_function_methods {
	usbf_status (*set_config)(struct usbf_function *, struct usbf_config *);
	usbf_status (*do_request)(struct usbf_function *,
	    usb_device_request_t *req, void **data);
};

struct usbf_function {
	struct device bdev;		/* base device */
	/* filled in by function driver */
	struct usbf_function_methods *methods;
};

#define USBF_EMPTY_STRING_ID		(USB_LANGUAGE_TABLE+1)
#define USBF_STRING_ID_MIN		(USB_LANGUAGE_TABLE+2)
#define USBF_STRING_ID_MAX		255

/*
 * USB function driver interface
 */

/* global */
const char	*usbf_errstr(usbf_status);

/* device */
void		 usbf_devinfo_setup(struct usbf_device *, u_int8_t, u_int8_t,
		     u_int8_t, u_int16_t, u_int16_t, u_int16_t, const char *,
		     const char *, const char *);
char		*usbf_devinfo_alloc(struct usbf_device *);
void		 usbf_devinfo_free(char *);
usb_device_descriptor_t *usbf_device_descriptor(struct usbf_device *);
usb_string_descriptor_t *usbf_string_descriptor(struct usbf_device *, u_int8_t);
usb_config_descriptor_t *usbf_config_descriptor(struct usbf_device *, u_int8_t);

/* configuration */
u_int8_t	 usbf_add_string(struct usbf_device *, const char *);
usbf_status	 usbf_add_config(struct usbf_device *, struct usbf_config **);
usbf_status	 usbf_add_config_desc(struct usbf_config *, usb_descriptor_t *,
		     usb_descriptor_t **);
usbf_status	 usbf_add_interface(struct usbf_config *, u_int8_t, u_int8_t,
		     u_int8_t, const char *, struct usbf_interface **);
usbf_status	 usbf_add_endpoint(struct usbf_interface *, u_int8_t,
		     u_int8_t, u_int16_t, u_int8_t, struct usbf_endpoint **);
usbf_status	 usbf_end_config(struct usbf_config *);
struct usbf_endpoint *usbf_config_endpoint(struct usbf_config *, u_int8_t);

/* interface */
int		 usbf_interface_number(struct usbf_interface *);
struct usbf_endpoint *usbf_iface_endpoint(struct usbf_interface *, u_int8_t);

/* endpoint */
u_int8_t	 usbf_endpoint_address(struct usbf_endpoint *);
u_int8_t	 usbf_endpoint_attributes(struct usbf_endpoint *);
#define usbf_endpoint_index(e)	UE_GET_ADDR(usbf_endpoint_address((e)))
#define usbf_endpoint_dir(e)	UE_GET_DIR(usbf_endpoint_address((e)))
#define usbf_endpoint_type(e)	UE_GET_XFERTYPE(usbf_endpoint_attributes((e)))

/* pipe */
usbf_status	 usbf_open_pipe(struct usbf_interface *, u_int8_t,
		     struct usbf_pipe **);
void		 usbf_abort_pipe(struct usbf_pipe *);
void		 usbf_close_pipe(struct usbf_pipe *);
void		 usbf_stall_pipe(struct usbf_pipe *);

/* transfer */
struct usbf_xfer *usbf_alloc_xfer(struct usbf_device *);
void		 usbf_free_xfer(struct usbf_xfer *);
void		*usbf_alloc_buffer(struct usbf_xfer *, u_int32_t);
void		 usbf_free_buffer(struct usbf_xfer *);
void		 usbf_setup_xfer(struct usbf_xfer *, struct usbf_pipe *,
		     void *, void *, u_int32_t, u_int16_t,
		     u_int32_t, usbf_callback);
void		 usbf_setup_default_xfer(struct usbf_xfer *, struct usbf_pipe *,
		     void *, usb_device_request_t *, u_int16_t,
		     u_int32_t, usbf_callback);
void		 usbf_get_xfer_status(struct usbf_xfer *, void **,
		     void **, u_int32_t *, usbf_status *);
usbf_status	 usbf_transfer(struct usbf_xfer *);

/*
 * The usbf_task structure describes a task to be perfomed in process
 * context, i.e. the USB device's task thread.  This is normally used by
 * USB function drivers that need to perform tasks in a process context.
 */
struct usbf_task {
	TAILQ_ENTRY(usbf_task) next;
	void (*fun)(void *);
	void *arg;
	char onqueue;
};

void usbf_add_task(struct usbf_device *, struct usbf_task *);
void usbf_rem_task(struct usbf_device *, struct usbf_task *);
#define usbf_init_task(t, f, a) ((t)->fun=(f), (t)->arg=(a), (t)->onqueue=0)

#endif

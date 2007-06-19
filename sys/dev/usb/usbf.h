/*	$OpenBSD: usbf.h,v 1.3 2007/06/19 11:52:07 mbalmer Exp $	*/

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

typedef struct usbf_function	*usbf_function_handle;
typedef struct usbf_bus		*usbf_bus_handle;
typedef struct usbf_device	*usbf_device_handle;
typedef struct usbf_config	*usbf_config_handle;
typedef struct usbf_interface	*usbf_interface_handle;
typedef struct usbf_endpoint	*usbf_endpoint_handle;
typedef struct usbf_pipe	*usbf_pipe_handle;
typedef struct usbf_xfer	*usbf_xfer_handle;
typedef void			*usbf_private_handle;

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

typedef void (*usbf_callback)(usbf_xfer_handle, usbf_private_handle,
    usbf_status);

/*
 * Attach USB function at the logical device.
 */
struct usbf_attach_arg {
	usbf_device_handle device;
};

struct usbf_function_methods {
	usbf_status (*set_config)(usbf_function_handle, usbf_config_handle);
	usbf_status (*do_request)(usbf_function_handle,
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
void		 usbf_devinfo_setup(usbf_device_handle, u_int8_t, u_int8_t,
		     u_int8_t, u_int16_t, u_int16_t, u_int16_t, const char *,
		     const char *, const char *);
char		*usbf_devinfo_alloc(usbf_device_handle);
void		 usbf_devinfo_free(char *);
usb_device_descriptor_t *usbf_device_descriptor(usbf_device_handle);
usb_string_descriptor_t *usbf_string_descriptor(usbf_device_handle, u_int8_t);
usb_config_descriptor_t *usbf_config_descriptor(usbf_device_handle, u_int8_t);

/* configuration */
u_int8_t	 usbf_add_string(usbf_device_handle, const char *);
usbf_status	 usbf_add_config(usbf_device_handle, usbf_config_handle *);
usbf_status	 usbf_add_config_desc(usbf_config_handle, usb_descriptor_t *,
		     usb_descriptor_t **);
usbf_status	 usbf_add_interface(usbf_config_handle, u_int8_t, u_int8_t,
		     u_int8_t, const char *, usbf_interface_handle *);
usbf_status	 usbf_add_endpoint(usbf_interface_handle, u_int8_t,
		     u_int8_t, u_int16_t, u_int8_t, usbf_endpoint_handle *);
usbf_status	 usbf_end_config(usbf_config_handle);
usbf_endpoint_handle usbf_config_endpoint(usbf_config_handle, u_int8_t);

/* interface */
int		 usbf_interface_number(usbf_interface_handle);
usbf_endpoint_handle usbf_iface_endpoint(usbf_interface_handle, u_int8_t);

/* endpoint */
u_int8_t	 usbf_endpoint_address(usbf_endpoint_handle);
u_int8_t	 usbf_endpoint_attributes(usbf_endpoint_handle);
#define usbf_endpoint_index(e)	UE_GET_ADDR(usbf_endpoint_address((e)))
#define usbf_endpoint_dir(e)	UE_GET_DIR(usbf_endpoint_address((e)))
#define usbf_endpoint_type(e)	UE_GET_XFERTYPE(usbf_endpoint_attributes((e)))

/* pipe */
usbf_status	 usbf_open_pipe(usbf_interface_handle, u_int8_t,
		     usbf_pipe_handle *);
void		 usbf_abort_pipe(usbf_pipe_handle);
void		 usbf_close_pipe(usbf_pipe_handle);
void		 usbf_stall_pipe(usbf_pipe_handle);

/* transfer */
usbf_xfer_handle usbf_alloc_xfer(usbf_device_handle);
void		 usbf_free_xfer(usbf_xfer_handle);
void		*usbf_alloc_buffer(usbf_xfer_handle, u_int32_t);
void		 usbf_free_buffer(usbf_xfer_handle);
void		 usbf_setup_xfer(usbf_xfer_handle, usbf_pipe_handle,
		     usbf_private_handle, void *, u_int32_t, u_int16_t,
		     u_int32_t, usbf_callback);
void		 usbf_setup_default_xfer(usbf_xfer_handle, usbf_pipe_handle,
		     usbf_private_handle, usb_device_request_t *, u_int16_t,
		     u_int32_t, usbf_callback);
void		 usbf_get_xfer_status(usbf_xfer_handle, usbf_private_handle *,
		     void **, u_int32_t *, usbf_status *);
usbf_status	 usbf_transfer(usbf_xfer_handle);

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

void usbf_add_task(usbf_device_handle, struct usbf_task *);
void usbf_rem_task(usbf_device_handle, struct usbf_task *);
#define usbf_init_task(t, f, a) ((t)->fun=(f), (t)->arg=(a), (t)->onqueue=0)

#endif

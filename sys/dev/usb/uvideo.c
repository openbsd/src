/*
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
 * Copyright (c) 2008 Marcus Glocker <mglocker@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/reboot.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/uvideo.h>

#include <dev/video_if.h>

#define UVIDEO_DEBUG

#ifdef UVIDEO_DEBUG
int uvideo_debug = 1;
#define DPRINTF(l, x...) do { if ((l) <= uvideo_debug) printf(x); } while (0)
#else
#define DPRINTF(l, x...)
#endif

int		uvideo_enable(void *);
void		uvideo_disable(void *);

int		uvideo_open(void *, int);
int		uvideo_close(void *);

int		uvideo_find_vs_if(struct uvideo_softc *,
		    struct usb_attach_arg *, usb_config_descriptor_t *);
usbd_status	uvideo_vs_alloc(struct uvideo_softc *);
usbd_status	uvideo_vs_open(struct uvideo_softc *);
void		uvideo_vs_start(struct uvideo_softc *);
void		uvideo_vs_cb(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);
int		uvideo_vs_set_probe(struct uvideo_softc *, uint8_t *);
int		uvideo_vs_get_probe(struct uvideo_softc *, uint8_t *);
int		uvideo_vs_set_probe_commit(struct uvideo_softc *, uint8_t *);

int		uvideo_vs_decode_stream_header(struct uvideo_softc *,
		    uint8_t *, int);

void		uvideo_dump_desc_all(struct uvideo_softc *);
void		uvideo_dump_desc_vcheader(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_input(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_output(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_endpoint(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_interface(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_config(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_cs_endpoint(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_colorformat(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_frame_mjpeg(struct uvideo_softc *,
		    const usb_descriptor_t *);
void		uvideo_dump_desc_format_mjpeg(struct uvideo_softc *,
		    const usb_descriptor_t *);

int		uvideo_debug_file_open(struct uvideo_softc *);
void		uvideo_debug_file_write_sample(void *);

void		uvideo_hexdump(void *, int);

int             uvideo_match(struct device *, void *, void *);
void            uvideo_attach(struct device *, struct device *, void *);
int             uvideo_detach(struct device *, int);
int             uvideo_activate(struct device *, enum devact);

int		uvideo_querycap(void *, struct v4l2_capability *);
int		uvideo_s_fmt(void *, struct v4l2_format *);
int		uvideo_g_fmt(void *, struct v4l2_format *);
int		uvideo_reqbufs(void *, struct v4l2_requestbuffers *);

#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

const struct cfattach uvideo_ca = {
	sizeof(struct uvideo_softc),
	uvideo_match,
	uvideo_attach,
	uvideo_detach,
	uvideo_activate,
};

struct cfdriver uvideo_cd = {
	NULL,
	"uvideo",
	DV_DULL
};

usbd_status 
uvideo_usb_request(struct uvideo_softc * sc, u_int8_t type, u_int8_t request,
    u_int16_t value, u_int16_t index, u_int16_t length, u_int8_t * data);

struct video_hw_if uvideo_hw_if = {
	uvideo_open,		/* open */
	uvideo_close,		/* close */
	uvideo_querycap,	/* VIDIOC_QUERYCAP */
	uvideo_s_fmt,		/* VIDIOC_S_FMT */
	uvideo_g_fmt,		/* VIDIOC_G_FMT */
	uvideo_reqbufs,		/* VIDIOC_REQBUFS */
	NULL,			/* VIDIOC_QBUF */
	NULL			/* VIDIOC_DQBUF */
};

int
uvideo_match(struct device * parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return (UMATCH_NONE);

	if (id->bInterfaceClass == UICLASS_VIDEO &&
	    id->bInterfaceSubClass == UISUBCLASS_VIDEOCONTROL)
		return (UMATCH_VENDOR_PRODUCT_CONF_IFACE);

	return (UMATCH_NONE);
}

void
uvideo_attach(struct device * parent, struct device * self, void *aux)
{
	struct uvideo_softc *sc = (struct uvideo_softc *) self;
	struct usb_attach_arg *uaa = aux;
	usb_config_descriptor_t *cdesc;
	usbd_status     err;
	uint8_t probe_data[34];
	struct uvideo_sample_buffer *fb = &sc->sc_sample_buffer;

	sc->sc_udev = uaa->device;

	/* get the config descriptor */
	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
		    DEVNAME(sc));
		return;
	}

	uvideo_dump_desc_all(sc);

	if (uvideo_vs_set_probe(sc, probe_data))
		return;
	if (uvideo_vs_get_probe(sc, probe_data))
		return;
	if (uvideo_vs_set_probe_commit(sc, probe_data))
		return;

	uvideo_find_vs_if(sc, uaa, cdesc);

	err = uvideo_vs_alloc(sc);
	if (err != USBD_NORMAL_COMPLETION)
		return;

	err = uvideo_vs_open(sc);
	if (err != USBD_NORMAL_COMPLETION)
		return;

	fb->buf = malloc(32000, M_TEMP, M_NOWAIT);

	uvideo_debug_file_open(sc);
	usb_init_task(&sc->sc_task_write, uvideo_debug_file_write_sample, sc);

	uvideo_vs_start(sc);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, &sc->sc_dev);

	DPRINTF(1, "uvideo_attach: doing video_attach_mi\n");
	sc->sc_videodev = video_attach_mi(&uvideo_hw_if, sc, &sc->sc_dev);
}

int
uvideo_detach(struct device * self, int flags)
{
	struct uvideo_softc *sc = (struct uvideo_softc *) self;
	int             rv = 0;

	sc->sc_dying = 1;

	if (sc->sc_videodev != NULL)
		rv = config_detach(sc->sc_videodev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, &sc->sc_dev);

	return (rv);
}

int
uvideo_activate(struct device * self, enum devact act)
{
	struct uvideo_softc *sc = (struct uvideo_softc *) self;
	int             rv = 0;

	DPRINTF(1, "uvideo_activate: sc=%p\n", sc);

	switch (act) {
	case DVACT_ACTIVATE:
		break;
	case DVACT_DEACTIVATE:
		if (sc->sc_videodev != NULL)
			config_deactivate(sc->sc_videodev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uvideo_enable(void *v)
{
	struct uvideo_softc *sc = v;

	DPRINTF(1, "%s: uvideo_enable sc=%p\n", DEVNAME(sc), sc);

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_enabled)
		return (EBUSY);


	sc->sc_enabled = 1;

	return (0);
}

void
uvideo_disable(void *v)
{
	struct uvideo_softc *sc = v;

	DPRINTF(1, "%s: uvideo_disable sc=%p\n", DEVNAME(sc), sc);

	if (!sc->sc_enabled) {
		printf("uvideo_disable: already disabled!\n");
		return;
	}
	sc->sc_enabled = 0;
}

int
uvideo_open(void *addr, int flags)
{
	struct uvideo_softc *sc = addr;

	DPRINTF(1, "uvideo_open: sc=%p\n", sc);

	if (sc->sc_dying)
		return (EIO);

	return (0);
}

int
uvideo_close(void *addr)
{
	struct uvideo_softc *sc = addr;

	DPRINTF(1, "uvideo_close: sc=%p\n", sc);

	return (0);
}

int
uvideo_find_vs_if(struct uvideo_softc *sc, struct usb_attach_arg *uaa,
    usb_config_descriptor_t *cdesc)
{
	struct uvideo_stream_if *si = &sc->sc_curr_strm;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i, numalts;

	printf("%s: nifaces=%d\n", DEVNAME(sc), uaa->nifaces);

	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] == NULL)
			continue;

		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id == NULL)
			continue;
		printf("%s: bInterfaceNumber=%d, bAlternateSetting=%d, ",
		    DEVNAME(sc),
		    id->bInterfaceNumber, id->bAlternateSetting);

		numalts = usbd_get_no_alts(cdesc, id->bInterfaceNumber);
		printf("numalts=%d\n", numalts);

		if (id->bInterfaceNumber == 1) {	/* XXX baInterfaceNr */
			si->in_ifaceh = uaa->ifaces[i];
			si->numalts = numalts;
		}

		ed = usbd_interface2endpoint_descriptor(uaa->ifaces[i], i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for iface %d\n",
			    DEVNAME(sc), i);
			continue;
		}
		printf("%s: bEndpointAddress=0x%02x\n",
		    DEVNAME(sc), ed->bEndpointAddress);
	}

	for (i = 0; i < si->numalts; i++) {
		if (usbd_set_interface(si->in_ifaceh, i)) {
			printf("%s: could not set alt iface %d\n",
			    DEVNAME(sc), i);
			return (1);
		}

		ed = usbd_interface2endpoint_descriptor(si->in_ifaceh, 0);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for VS iface\n",
			    DEVNAME(sc));
			continue;
		}
		printf("%s: VS iface alt iface bEndpointAddress=0x%02x, "
		    "wMaxPacketSize=%d\n",
		    DEVNAME(sc),
		    ed->bEndpointAddress,
		    UGETW(ed->wMaxPacketSize));

		si->endpoint = ed->bEndpointAddress;
	}

	return (0);
}

usbd_status
uvideo_vs_alloc(struct uvideo_softc *sc)
{
	struct uvideo_stream_if *si = &sc->sc_curr_strm;

	DPRINTF(1, "%s: %s\n", DEVNAME(sc), __func__);

	si->sc = sc;

	si->xfer = usbd_alloc_xfer(sc->sc_udev);	
	if (si->xfer == NULL) {
		printf("%s: could not allocate VideoStream xfer!\n",
		    DEVNAME(sc));
		return (USBD_NOMEM);	
	}

	si->buf = usbd_alloc_buffer(si->xfer, 384 * UVIDEO_NFRAMES);
	if (si->buf == NULL) {
		printf("%s: could not allocate VideoStream buffer!\n",
		    DEVNAME(sc));
		return (USBD_NOMEM);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uvideo_vs_open(struct uvideo_softc *sc)
{
	struct uvideo_stream_if *si = &sc->sc_curr_strm;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;

	DPRINTF(1, "%s: %s\n", DEVNAME(sc), __func__);

	err = usbd_set_interface(si->in_ifaceh, 2);
	if (err != 0) {
		printf("%s: could not set alternate interface!\n",
		    DEVNAME(sc));
		return (err);
	}

	ed = usbd_interface2endpoint_descriptor(si->in_ifaceh, 0);
	if (ed == NULL) {
		printf("%s: no endpoint descriptor for VS iface\n",
		    DEVNAME(sc));
		return (USBD_NOMEM);
	}
	printf("%s: open pipe for bEndpointAddress=0x%02x (0x%02x), "
	    "wMaxPacketSize=%d\n",
	    DEVNAME(sc),
	    ed->bEndpointAddress,
	    si->endpoint,
	    UGETW(ed->wMaxPacketSize));

	err = usbd_open_pipe(si->in_ifaceh, si->endpoint, USBD_EXCLUSIVE_USE,
	    &si->in_pipeh);
	if (err) {
		printf("%s: could not open VS pipe: %s\n",
		    DEVNAME(sc), usbd_errstr(err));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

void
uvideo_vs_start(struct uvideo_softc *sc)
{
	struct uvideo_stream_if *si = &sc->sc_curr_strm;
	int i;

	DPRINTF(2, "%s: %s\n", DEVNAME(sc), __func__);

	for (i = 0; i < UVIDEO_NFRAMES; i++)
		si->size[i] = 384;

	bzero(si->buf, 384 * UVIDEO_NFRAMES);

	usbd_setup_isoc_xfer(
	    si->xfer,
	    si->in_pipeh,
	    si,
	    si->size,
	    UVIDEO_NFRAMES,
	    USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    uvideo_vs_cb);

	(void)usbd_transfer(si->xfer);
}

void
uvideo_vs_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct uvideo_stream_if *sc_curr_strm = priv;
	struct uvideo_softc *sc = sc_curr_strm->sc;
	int len, i, frame_size;
	uint8_t *frame;

	DPRINTF(2, "%s: %s\n", DEVNAME(sc), __func__);

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s\n", DEVNAME(sc), __func__,
		    usbd_errstr(status));
		return;
        }
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	DPRINTF(2, "%s: *** buffer len = %d\n", DEVNAME(sc), len);
	if (len == 0)
		goto skip;

	for (i = 0; i < UVIDEO_NFRAMES; i++) {
		frame = sc_curr_strm->buf + (i * 384);
		frame_size = sc_curr_strm->size[i];

		if (frame_size == 0)
			/* frame is empty */
			continue;

		uvideo_vs_decode_stream_header(sc, frame, frame_size);
	}

skip:	/* setup new transfer */
	uvideo_vs_start(sc);
}

int
uvideo_vs_set_probe(struct uvideo_softc *sc, uint8_t *probe_data)
{
	usb_device_request_t req;
	usbd_status err;
	uint16_t tmp;
	struct usb_video_probe_commit *pc;

	req.bmRequestType = UVIDEO_SET_IF;
	req.bRequest = SET_CUR;
	tmp = VS_PROBE_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, 1);
	USETW(req.wLength, 26);

	bzero(probe_data, sizeof(probe_data));
	pc = (struct usb_video_probe_commit *)probe_data;
	USETW(pc->bmHint, 8);
	//USETW(pc->bmHint, 1);
	pc->bFormatIndex = 1;
	pc->bFrameIndex = 1;

	err = usbd_do_request(sc->sc_udev, &req, probe_data);
	if (err) {
		printf("%s: could not send SET request: %s\n",
		    DEVNAME(sc), usbd_errstr(err));
		return (-1);
	}

	DPRINTF(1, "%s: SET probe control request successfully\n",
	    DEVNAME(sc));

	return (0);
}

int
uvideo_vs_get_probe(struct uvideo_softc *sc, uint8_t *probe_data)
{
	usb_device_request_t req;
	usbd_status err;
	uint16_t tmp;
	struct usb_video_probe_commit *pc;

	req.bmRequestType = UVIDEO_GET_IF;
	req.bRequest = GET_CUR;
	tmp = VS_PROBE_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, 1);
	USETW(req.wLength, 26);

	bzero(probe_data, sizeof(probe_data));
	pc = (struct usb_video_probe_commit *)probe_data;

	err = usbd_do_request(sc->sc_udev, &req, probe_data);
	if (err) {
		printf("%s: could not send GET request: %s\n",
		    DEVNAME(sc), usbd_errstr(err));
		return (-1);
	}

	DPRINTF(1, "%s: GET probe control request successfully\n",
	    DEVNAME(sc));

	DPRINTF(1, "bmHint=0x%02x\n", UGETW(pc->bmHint));
	DPRINTF(1, "bFormatIndex=0x%02x\n", pc->bFormatIndex);
	DPRINTF(1, "bFrameIndex=0x%02x\n", pc->bFrameIndex);
	DPRINTF(1, "dwFrameInterval=%d (ns)\n", UGETDW(pc->dwFrameInterval));
	DPRINTF(1, "wKeyFrameRate=%d\n", UGETW(pc->wKeyFrameRate));
	DPRINTF(1, "wPFrameRate=0x%d\n", UGETW(pc->wPFrameRate));
	DPRINTF(1, "wCompQuality=0x%d\n", UGETW(pc->wCompQuality));
	DPRINTF(1, "wCompWindowSize=0x%04x\n", UGETW(pc->wCompWindowSize));
	DPRINTF(1, "wDelay=%d (ms)\n", UGETW(pc->wDelay));
	DPRINTF(1, "dwMaxVideoFrameSize=%d (bytes)\n",
	    UGETDW(pc->dwMaxVideoFrameSize));
	DPRINTF(1, "dwMaxPayloadTransferSize=%d (bytes)\n",
	    UGETDW(pc->dwMaxPayloadTransferSize));

	return (0);
}

int
uvideo_vs_set_probe_commit(struct uvideo_softc *sc, uint8_t *probe_data)
{
	usb_device_request_t req;
	usbd_status err;
	uint16_t tmp;

	req.bmRequestType = UVIDEO_SET_IF;
	req.bRequest = SET_CUR;
	tmp = VS_COMMIT_CONTROL;
	tmp = tmp << 8;
	USETW(req.wValue, tmp);
	USETW(req.wIndex, 1);
	USETW(req.wLength, 26);

	err = usbd_do_request(sc->sc_udev, &req, probe_data);
	if (err) {
		printf("%s: could not send SET commit request: %s\n",
		    DEVNAME(sc), usbd_errstr(err));
		return (-1);
	}

	DPRINTF(1, "%s: SET probe commit request successfully\n",
	    DEVNAME(sc));

	return (0);
}

int
uvideo_vs_decode_stream_header(struct uvideo_softc *sc, uint8_t *frame,
    int frame_size)
{
	struct uvideo_sample_buffer *fb = &sc->sc_sample_buffer;
	int header_len, header_flags, fragment_len;

	if (frame_size < 2)
		/* frame too small to contain a valid stream header */
		return (-1);

	header_len = frame[0];
	header_flags = frame[1];

	DPRINTF(2, "%s: header_len = %d\n", DEVNAME(sc), header_len);

	if (header_len != 12)
		/* frame header is 12 bytes long */
		return (-1);
	if (header_len == frame_size && !(header_flags & UVIDEO_STREAM_EOF)) {
		/* stream header without payload and no EOF */
		return (-1);
	}


	DPRINTF(2, "%s: frame_size = %d\n", DEVNAME(sc), frame_size);

	if (header_flags & UVIDEO_STREAM_FID) {
		DPRINTF(2, "%s: %s: FID ON (0x%02x)\n",
		    DEVNAME(sc), __func__,
		    header_flags & UVIDEO_STREAM_FID);
	} else {
		DPRINTF(2, "%s: %s: FID OFF (0x%02x)\n",
		    DEVNAME(sc), __func__,
		    header_flags & UVIDEO_STREAM_FID);
	}

	if (fb->fragment == 0) {
		/* first fragment for a sample */
		fb->fragment = 1;
		fb->fid = header_flags & UVIDEO_STREAM_FID;
		fb->offset = 0;
	} else {
		/* continues fragment for a sample, check consistency */
		if (fb->fid != (header_flags & UVIDEO_STREAM_FID)) {
			DPRINTF(1, "%s: %s: wrong FID, ignore last sample!\n",
			    DEVNAME(sc), __func__);
			fb->fragment = 1;
			fb->fid = header_flags & UVIDEO_STREAM_FID;
			fb->offset = 0;
		}
	}

	/* save sample fragment */
	fragment_len = frame_size - header_len;
	bcopy(frame + header_len, fb->buf + fb->offset, fragment_len);
	fb->offset += fragment_len;

	if (header_flags & UVIDEO_STREAM_EOF) {
		/* got a full sample */
		DPRINTF(2, "%s: %s: EOF (sample size = %d bytes)\n",
		    DEVNAME(sc), __func__, fb->offset);
#ifdef UVIDEO_DEBUG
		/* do the file write in process context */
		usb_rem_task(sc->sc_udev, &sc->sc_task_write);
		usb_add_task(sc->sc_udev, &sc->sc_task_write);
#endif
		fb->fragment = 0;
		fb->fid = 0;
//		fb->offset = 0;
	}

	return (0);
}

int
uvideo_querycap(void *v, struct v4l2_capability * caps)
{
	struct uvideo_softc *sc = v;

	bzero(caps, sizeof(caps));
	strlcpy(caps->driver, DEVNAME(sc),
		sizeof(caps->driver));
	strncpy(caps->card, "Generic USB video class device",
		sizeof(caps->card));
	strncpy(caps->bus_info, "usb", sizeof(caps->bus_info));

	caps->version = 1;
	caps->capabilities = V4L2_CAP_VIDEO_CAPTURE
		| V4L2_CAP_STREAMING
		| V4L2_CAP_READWRITE;

	return (0);
}

int
uvideo_s_fmt(void *v, struct v4l2_format * fmt)
{
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	return (0);
}

int
uvideo_g_fmt(void *v, struct v4l2_format * fmt)
{
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return (EINVAL);

	return (0);
}

int
uvideo_reqbufs(void *v, struct v4l2_requestbuffers * rb)
{
	return (0);
}

void
uvideo_dump_desc_all(struct uvideo_softc *sc)
{
	usbd_desc_iter_t iter;
	const usb_descriptor_t *desc;

	usb_desc_iter_init(sc->sc_udev, &iter);
	desc = usb_desc_iter_next(&iter);
	while (desc) {
		printf("bLength=%d\n", desc->bLength);
		printf("bDescriptorType=0x%02x", desc->bDescriptorType);

		switch (desc->bDescriptorType) {
		case UDESC_CS_INTERFACE:
			printf(" (CS_INTERFACE)\n");

			switch (desc->bDescriptorSubtype) {
			case UDESCSUB_VC_HEADER:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VC_HEADER)\n");
				if (desc->bLength == 13) {
					printf("|\n");
					uvideo_dump_desc_vcheader(sc, desc);
				}
				break;
			case UDESCSUB_VC_INPUT_TERMINAL:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VC_INPUT_TERMINAL)\n");
				printf("|\n");
				uvideo_dump_desc_input(sc, desc);
				break;
			case UDESCSUB_VC_OUTPUT_TERMINAL:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VC_OUTPUT)\n");
				printf("|\n");
				uvideo_dump_desc_output(sc, desc);
				break;
			case UDESCSUB_VC_SELECTOR_UNIT:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VC_SELECTOR_UNIT)\n");
				/* TODO */
				break;
			case UDESCSUB_VC_PROCESSING_UNIT:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VC_PROCESSING_UNIT)\n");
				/* TODO */
				break;
			case UDESCSUB_VC_EXTENSION_UNIT:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				if (desc->bLength == 11) {
					printf(" (UDESCSUB_VS_FORMAT_MJPEG)\n");
					printf("|\n");
					uvideo_dump_desc_format_mjpeg(sc, desc);
				} else {
					printf(" (UDESCSUB_VC_EXTENSION_UNIT)\n");
					/* TODO */
				}
				break;
			case UDESCSUB_VS_FRAME_MJPEG:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VS_FRAME_MJPEG)\n");
				if (desc->bLength > 26) {
					printf("|\n");
					uvideo_dump_desc_frame_mjpeg(sc, desc);
				}
				break;
			case UDESCSUB_VS_COLORFORMAT:
				printf("bDescriptorSubtype=0x%02x",
				   desc->bDescriptorSubtype);
				printf(" (UDESCSUB_VS_COLORFORMAT)\n");
				printf("|\n");
				uvideo_dump_desc_colorformat(sc, desc);
				break;
			}

			break;
		case UDESC_CS_ENDPOINT:
			printf(" (UDESC_CS_ENDPOINT)\n");

			switch (desc->bDescriptorSubtype) {
			case EP_INTERRUPT:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (EP_INTERRUPT)\n");
				printf("|\n");
				uvideo_dump_desc_cs_endpoint(sc, desc);
				break;
			case EP_GENERAL:
				printf("bDescriptorSubtype=0x%02x",
				    desc->bDescriptorSubtype);
				printf(" (EP_GENERAL)\n");
				printf("|\n");
				uvideo_dump_desc_cs_endpoint(sc, desc);
				break;
			}

			break;
		case UDESC_CONFIG:
			printf(" (UDESC_CONFIG)\n");
			printf("|\n");
			uvideo_dump_desc_config(sc, desc);
			break;
		case UDESC_ENDPOINT:
			printf(" (UDESC_ENDPOINT)\n");
			printf("|\n");
			uvideo_dump_desc_endpoint(sc, desc);
			break;
		case UDESC_INTERFACE:
			printf(" (UDESC_INTERFACE)\n");
			printf("|\n");
			uvideo_dump_desc_interface(sc, desc);
			break;
		default:
			printf(" (unknown)\n");
			break;
		}

		printf("\n");

		desc = usb_desc_iter_next(&iter);
	}	

}

void
uvideo_dump_desc_vcheader(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_header_desc *d;

	d = (struct usb_video_header_desc *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bcdUVC=0x%04x\n", UGETW(d->bcdUVC));
	printf("wTotalLength=%d\n", UGETW(d->wTotalLength));
	printf("dwClockFrequency=%d\n", UGETDW(d->dwClockFrequency));
	printf("bInCollection=0x%02x\n", d->bInCollection);
	printf("baInterfaceNr=0x%02x\n", d->baInterfaceNr);
}

void
uvideo_dump_desc_input(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_input_terminal_desc *d;

	d = (struct usb_video_input_terminal_desc *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bTerminalID=0x%02x\n", d->bTerminalID);
	printf("wTerminalType=0x%04x\n", UGETW(d->wTerminalType));
	printf("bAssocTerminal=0x%02x\n", d->bAssocTerminal);
	printf("iTerminal=0x%02x\n", d->iTerminal);
}

void
uvideo_dump_desc_output(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_output_terminal_desc *d;

	d = (struct usb_video_output_terminal_desc *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bTerminalID=0x%02x\n", d->bTerminalID);
	printf("bAssocTerminal=0x%02x\n", d->bAssocTerminal);
	printf("bSourceID=0x%02x\n", d->bSourceID);
	printf("iTerminal=0x%02x\n", d->iTerminal);

}

void
uvideo_dump_desc_endpoint(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	usb_endpoint_descriptor_t *d;

	d = (usb_endpoint_descriptor_t *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bEndpointAddress=0x%02x", d->bEndpointAddress);
	if (UE_GET_DIR(d->bEndpointAddress) == UE_DIR_IN)
		printf(" (IN)\n");
	if (UE_GET_DIR(d->bEndpointAddress) == UE_DIR_OUT)
		printf(" (OUT)\n");
	printf("bmAttributes=0x%02x", d->bmAttributes);
	if (UE_GET_XFERTYPE(d->bmAttributes) == UE_ISOCHRONOUS) {
		printf(" (UE_ISOCHRONOUS,");
		if (UE_GET_ISO_TYPE(d->bmAttributes) == UE_ISO_ASYNC)
			printf(" UE_ISO_ASYNC)\n");
		if (UE_GET_ISO_TYPE(d->bmAttributes) == UE_ISO_ADAPT)
			printf(" UE_ISO_ADAPT)\n");
		if (UE_GET_ISO_TYPE(d->bmAttributes) == UE_ISO_SYNC)
			printf(" UE_ISO_SYNC)\n");
	}
	if (UE_GET_XFERTYPE(d->bmAttributes) == UE_CONTROL)
		printf(" (UE_CONTROL)\n");
	if (UE_GET_XFERTYPE(d->bmAttributes) == UE_BULK)
		printf(" (UE_BULK)\n");
	if (UE_GET_XFERTYPE(d->bmAttributes) == UE_INTERRUPT)
		printf(" (UE_INTERRUPT)\n");
	printf("wMaxPacketsize=%d\n", UGETW(d->wMaxPacketSize));
	printf("bInterval=0x%02x\n", d->bInterval);
}

void
uvideo_dump_desc_interface(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	usb_interface_descriptor_t *d;

	d = (usb_interface_descriptor_t *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bInterfaceNumber=0x%02x\n", d->bInterfaceNumber);
	printf("bAlternateSetting=0x%02x\n", d->bAlternateSetting);
	printf("bNumEndpoints=%d\n", d->bNumEndpoints);
	printf("bInterfaceClass=0x%02x\n", d->bInterfaceClass);
	printf("bInterfaceSubClass=0x%02x\n", d->bInterfaceSubClass);
	printf("bInterfaceProtocol=0x%02x\n", d->bInterfaceProtocol);
	printf("iInterface=0x%02x\n", d->iInterface);
}

void
uvideo_dump_desc_config(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	usb_config_descriptor_t *d;

	d = (usb_config_descriptor_t *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("wTotalLength=0x%02x\n", d->wTotalLength);
	printf("bNumInterface=0x%02x\n", d->bNumInterface);
	printf("bConfigurationValue=0x%02x\n", d->bConfigurationValue);
	printf("iConfiguration=0x%02x\n", d->iConfiguration);
	printf("bmAttributes=0x%02x\n", d->bmAttributes);
	printf("bMaxPower=0x%02x\n", d->bMaxPower);
}

void
uvideo_dump_desc_cs_endpoint(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_vc_endpoint_desc *d;

	d = (struct usb_video_vc_endpoint_desc *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("wMaxTransferSize=%d\n", UGETW(d->wMaxTransferSize));
}

void
uvideo_dump_desc_colorformat(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_color_matching_descr *d;

	d = (struct usb_video_color_matching_descr *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bColorPrimaries=0x%02x\n", d->bColorPrimaries);
	printf("bTransferCharacteristics=0x%02x\n",
	    d->bTransferCharacteristics);
	printf("bMatrixCoefficients=0x%02x\n", d->bMatrixCoefficients);
}

void
uvideo_dump_desc_frame_mjpeg(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_frame_descriptor *d;

	d = (struct usb_video_frame_descriptor *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bFrameIndex=0x%02x\n", d->bFrameIndex);
	printf("bmCapabilities=0x%02x\n", d->bmCapabilities);
	printf("wWidth=0x%d\n", UGETW(d->wWidth));
	printf("wHeight=0x%d\n", UGETW(d->wHeight));
	printf("dwMinBitRate=0x%08x\n", UGETDW(d->dwMaxBitRate));
	printf("dwMaxVideoFrameBufferSize=0x%08x\n",
	    UGETDW(d->dwMaxVideoFrameBufferSize));
	printf("dwDefaultFrameInterval=0x%d\n",
	    UGETDW(d->dwDefaultFrameInterval));
	printf("bFrameIntervalType=0x%02x\n", d->bFrameIntervalType);
}

void
uvideo_dump_desc_format_mjpeg(struct uvideo_softc *sc,
    const usb_descriptor_t *desc)
{
	struct usb_video_format_mjpeg_descriptor *d;

	d = (struct usb_video_format_mjpeg_descriptor *)(uint8_t *)desc;

	printf("bLength=%d\n", d->bLength);
	printf("bDescriptorType=0x%02x\n", d->bDescriptorType);
	printf("bDescriptorSubtype=0x%02x\n", d->bDescriptorSubtype);
	printf("bFormatIndex=0x%02x\n", d->bFormatIndex);
	printf("bNumFrameDescriptors=0x%02x\n", d->bNumFrameDescriptors);
	printf("bmFlags=0x%02x\n", d->bmFlags);
	printf("bDefaultFrameIndex=0x%02x\n", d->bDefaultFrameIndex);
	printf("bAspectRatioX=0x%02x\n", d->bAspectRatioX);
	printf("bAspectRatioY=0x%02x\n", d->bAspectRatioY);
	printf("bmInterlaceFlags=0x%02x\n", d->bmInterlaceFlags);
	printf("bCopyProtect=0x%02x\n", d->bCopyProtect);
}

int
uvideo_debug_file_open(struct uvideo_softc *sc)
{
	struct proc *p = curproc;
	struct nameidata nd;
	char name[] = "/uvideo.mjpeg";
	int error;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name, p);
	error = vn_open(&nd, O_CREAT | FWRITE | O_NOFOLLOW, S_IRUSR | S_IWUSR);
	if (error) {
		return (-1);
	}

	sc->sc_vp = nd.ni_vp;
	VOP_UNLOCK(sc->sc_vp, 0, p);
	if (nd.ni_vp->v_type != VREG) {
		vn_close(nd.ni_vp, FWRITE, p->p_ucred, p);
		return (-1);
	}

	DPRINTF(1, "%s: %s: created debug file %s\n",
	    DEVNAME(sc), __func__, name);

	return (0);
}

void
uvideo_debug_file_write_sample(void *arg)
{
	struct uvideo_softc *sc = arg;
	struct uvideo_sample_buffer *sb = &sc->sc_sample_buffer;
	struct proc *p = curproc;
	int error;

	if (sc->sc_vp == NULL) {
		printf("%s: %s: no file open!\n", DEVNAME(sc));
		return;
	}

	error = vn_rdwr(UIO_WRITE, sc->sc_vp, sb->buf, sb->offset, (off_t)0,
	    UIO_SYSSPACE, IO_APPEND|IO_UNIT, p->p_ucred, NULL, p);

	if (error)
		DPRINTF(1, "vn_rdwr error!\n");
}

void
uvideo_hexdump(void *buf, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			printf("%s%5i:", i ? "\n" : "", i);
		if (i % 4 == 0)
			printf(" ");
		printf("%02x", (int)*((u_char *)buf + i));
	}
	printf("\n");
}

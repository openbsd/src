/*	$OpenBSD: uaudio.c,v 1.108 2014/12/13 21:05:33 doug Exp $ */
/*	$NetBSD: uaudio.c,v 1.90 2004/10/29 17:12:53 kent Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * USB audio specs: http://www.usb.org/developers/devclass_docs/audio10.pdf
 *                  http://www.usb.org/developers/devclass_docs/frmts10.pdf
 *                  http://www.usb.org/developers/devclass_docs/termt10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/uaudioreg.h>

/* #define UAUDIO_DEBUG */
#ifdef UAUDIO_DEBUG
#define DPRINTF(x)	do { if (uaudiodebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uaudiodebug>(n)) printf x; } while (0)
int	uaudiodebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UAUDIO_NCHANBUFS 3	/* number of outstanding request */
#define UAUDIO_MIN_FRAMES 2	/* ms of sound in each request */
#define UAUDIO_MAX_FRAMES 16
#define UAUDIO_NSYNCBUFS 3	/* number of outstanding sync requests */

#define UAUDIO_MAX_ALTS  32	/* max alt settings allowed by driver */

#define MIX_MAX_CHAN 8
struct mixerctl {
	u_int16_t	wValue[MIX_MAX_CHAN]; /* using nchan */
	u_int16_t	wIndex;
	u_int8_t	nchan;
	u_int8_t	type;
#define MIX_ON_OFF	1
#define MIX_SIGNED_16	2
#define MIX_UNSIGNED_16	3
#define MIX_SIGNED_8	4
#define MIX_SELECTOR	5
#define MIX_SIZE(n) ((n) == MIX_SIGNED_16 || (n) == MIX_UNSIGNED_16 ? 2 : 1)
#define MIX_UNSIGNED(n) ((n) == MIX_UNSIGNED_16)
	int		minval, maxval;
	u_int		delta;
	u_int8_t	class;
	char		ctlname[MAX_AUDIO_DEV_LEN];
	char		*ctlunit;
};
#define MAKE(h,l) (((h) << 8) | (l))

struct as_info {
	u_int8_t	alt;
	u_int8_t	encoding;
	u_int8_t	attributes; /* Copy of bmAttributes of
				     * usb_audio_streaming_endpoint_descriptor
				     */
	struct usbd_interface *ifaceh;
	const usb_interface_descriptor_t *idesc;
	const struct usb_endpoint_descriptor_audio *edesc;
	const struct usb_endpoint_descriptor_audio *edesc1;
	const struct usb_audio_streaming_type1_descriptor *asf1desc;
	int		sc_busy;	/* currently used */
};

struct chan {
	void	(*intr)(void *);	/* DMA completion intr handler */
	void	*arg;		/* arg for intr() */
	struct usbd_pipe *pipe;
	struct usbd_pipe *sync_pipe;

	u_int	sample_size;
	u_int	sample_rate;
	u_int	bytes_per_frame;
	u_int	max_bytes_per_frame;
	u_int	fraction;	/* fraction/frac_denom is the extra samples/frame */
	u_int	frac_denom;	/* denominator for fractional samples */
	u_int	residue;	/* accumulates the fractional samples */
	u_int	nframes;	/* # of frames per transfer */
	u_int	nsync_frames;	/* # of frames per sync transfer */
	u_int	usb_fps;
	u_int	maxpktsize;
	u_int	reqms;		/* usb request data duration, in ms */
	u_int	hi_speed;

	u_char	*start;		/* upper layer buffer start */
	u_char	*end;		/* upper layer buffer end */
	u_char	*cur;		/* current position in upper layer buffer */
	int	blksize;	/* chunk size to report up */
	int	transferred;	/* transferred bytes not reported up */

	int	altidx;		/* currently used altidx */

	int	curchanbuf;
	int	cursyncbuf;

	struct chanbuf {
		struct chan	*chan;
		struct usbd_xfer *xfer;
		u_char		*buffer;
		u_int16_t	sizes[UAUDIO_MAX_FRAMES];
		u_int16_t	offsets[UAUDIO_MAX_FRAMES];
		u_int16_t	size;
	} chanbufs[UAUDIO_NCHANBUFS];

	struct syncbuf {
		struct chan	*chan;
		struct usbd_xfer *xfer;
		u_char		*buffer;
		u_int16_t	sizes[UAUDIO_MAX_FRAMES];
		u_int16_t	offsets[UAUDIO_MAX_FRAMES];
		u_int16_t	size;
	} syncbufs[UAUDIO_NSYNCBUFS];

	struct uaudio_softc *sc; /* our softc */
};

#define UAUDIO_FLAG_BAD_AUDIO	 0x0001	/* claims audio class, but isn't */
#define UAUDIO_FLAG_NO_FRAC	 0x0002	/* don't use fractional samples */
#define UAUDIO_FLAG_NO_XU	 0x0004	/* has broken extension unit */
#define UAUDIO_FLAG_BAD_ADC	 0x0008	/* bad audio spec version number */
#define UAUDIO_FLAG_VENDOR_CLASS 0x0010	/* claims vendor class but works */
#define UAUDIO_FLAG_DEPENDENT	 0x0020	/* play and record params must equal */
#define UAUDIO_FLAG_EMU0202	 0x0040

struct uaudio_devs {
	struct usb_devno	 uv_dev;
	int			 flags;
} uaudio_devs[] = {
	{ { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ADA70 },
		UAUDIO_FLAG_BAD_ADC } ,
	{ { USB_VENDOR_ALTEC, USB_PRODUCT_ALTEC_ASC495 },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_3G },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_3GS },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_4_GSM },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_4_CDMA },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPHONE_4S },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPOD_TOUCH },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPOD_TOUCH_2G },
		UAUDIO_FLAG_BAD_AUDIO },	
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPOD_TOUCH_3G },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPOD_TOUCH_4G },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPAD },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_APPLE, USB_PRODUCT_APPLE_IPAD2 },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_CREATIVE, USB_PRODUCT_CREATIVE_EMU0202 },
		UAUDIO_FLAG_VENDOR_CLASS | UAUDIO_FLAG_EMU0202 |
		UAUDIO_FLAG_DEPENDENT },
	{ { USB_VENDOR_DALLAS, USB_PRODUCT_DALLAS_J6502 },
		UAUDIO_FLAG_NO_XU | UAUDIO_FLAG_BAD_ADC },
	{ { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_QUICKCAMNBDLX },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_QUICKCAMPRONB },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_QUICKCAMPRO4K },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_LOGITECH, USB_PRODUCT_LOGITECH_QUICKCAMZOOM },
		UAUDIO_FLAG_BAD_AUDIO },
	{ { USB_VENDOR_TELEX, USB_PRODUCT_TELEX_MIC1 },
		UAUDIO_FLAG_NO_FRAC }
};
#define uaudio_lookup(v, p) \
	((struct uaudio_devs *)usb_lookup(uaudio_devs, v, p))

struct uaudio_softc {
	struct device	 sc_dev;	/* base device */
	struct usbd_device *sc_udev;	/* USB device */
	int		 sc_ac_iface;	/* Audio Control interface */
	struct chan	 sc_playchan;	/* play channel */
	struct chan	 sc_recchan;	/* record channel */
	int		 sc_nullalt;
	int		 sc_audio_rev;
	struct as_info	*sc_alts;	/* alternate settings */
	int		 sc_nalts;	/* # of alternate settings */
	int		 sc_altflags;
#define HAS_8		 0x01
#define HAS_16		 0x02
#define HAS_8U		 0x04
#define HAS_ALAW	 0x08
#define HAS_MULAW	 0x10
#define UA_NOFRAC	 0x20		/* don't do sample rate adjustment */
#define HAS_24		 0x40
	int		 sc_mode;	/* play/record capability */
	struct audio_encoding *sc_encs;
	int		 sc_nencs;
	struct mixerctl *sc_ctls;	/* mixer controls */
	int		 sc_nctls;	/* # of mixer controls */
	int		 sc_quirks;
};

struct terminal_list {
	int size;
	uint16_t terminals[1];
};
#define TERMINAL_LIST_SIZE(N)	(offsetof(struct terminal_list, terminals) \
				+ sizeof(uint16_t) * (N))

struct io_terminal {
	union {
		const usb_descriptor_t *desc;
		const struct usb_audio_input_terminal *it;
		const struct usb_audio_output_terminal *ot;
		const struct usb_audio_mixer_unit *mu;
		const struct usb_audio_selector_unit *su;
		const struct usb_audio_feature_unit *fu;
		const struct usb_audio_processing_unit *pu;
		const struct usb_audio_extension_unit *eu;
	} d;
	int inputs_size;
	struct terminal_list **inputs; /* list of source input terminals */
	struct terminal_list *output; /* list of destination output terminals */
	int direct;		/* directly connected to an output terminal */
};

#define UAC_OUTPUT	0
#define UAC_INPUT	1
#define UAC_EQUAL	2
#define UAC_RECORD	3
#define UAC_NCLASSES	4
#ifdef UAUDIO_DEBUG
const char *uac_names[] = {
	AudioCoutputs, AudioCinputs, AudioCequalization, AudioCrecord,
};
#endif

usbd_status uaudio_identify_ac
	(struct uaudio_softc *, const usb_config_descriptor_t *);
usbd_status uaudio_identify_as
	(struct uaudio_softc *, const usb_config_descriptor_t *);
usbd_status uaudio_process_as
	(struct uaudio_softc *, const char *, int *, int,
	 const usb_interface_descriptor_t *);

void	uaudio_add_alt(struct uaudio_softc *, const struct as_info *);

void	uaudio_create_encodings(struct uaudio_softc *);

const usb_interface_descriptor_t *uaudio_find_iface
	(const char *, int, int *, int, int);

void	uaudio_mixer_add_ctl(struct uaudio_softc *, struct mixerctl *);
char	*uaudio_id_name
	(struct uaudio_softc *, const struct io_terminal *, int);
uByte	uaudio_get_cluster_nchan
	(int, const struct io_terminal *);
void	uaudio_add_input
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_output
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_mixer
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_selector
	(struct uaudio_softc *, const struct io_terminal *, int);
#ifdef UAUDIO_DEBUG
const char *uaudio_get_terminal_name(int);
#endif
int	uaudio_determine_class
	(const struct io_terminal *, struct mixerctl *);
const char *uaudio_feature_name
	(const struct io_terminal *, struct mixerctl *);
void	uaudio_add_feature
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_processing_updown
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_processing
	(struct uaudio_softc *, const struct io_terminal *, int);
void	uaudio_add_extension
	(struct uaudio_softc *, const struct io_terminal *, int);
struct terminal_list *uaudio_merge_terminal_list
	(const struct io_terminal *);
struct terminal_list *uaudio_io_terminaltype
	(int, struct io_terminal *, int);
usbd_status uaudio_identify
	(struct uaudio_softc *, const usb_config_descriptor_t *);

int	uaudio_signext(int, int);
int	uaudio_unsignext(int, int);
int	uaudio_value2bsd(struct mixerctl *, int);
int	uaudio_bsd2value(struct mixerctl *, int);
int	uaudio_get(struct uaudio_softc *, int, int, int, int, int);
int	uaudio_ctl_get
	(struct uaudio_softc *, int, struct mixerctl *, int);
void	uaudio_set
	(struct uaudio_softc *, int, int, int, int, int, int);
void	uaudio_ctl_set
	(struct uaudio_softc *, int, struct mixerctl *, int, int);

usbd_status uaudio_set_speed(struct uaudio_softc *, int, u_int);
void	uaudio_set_speed_emu0202(struct chan *ch);

usbd_status uaudio_chan_open(struct uaudio_softc *, struct chan *);
void	uaudio_chan_close(struct uaudio_softc *, struct chan *);
usbd_status uaudio_chan_alloc_buffers
	(struct uaudio_softc *, struct chan *);
void	uaudio_chan_free_buffers(struct uaudio_softc *, struct chan *);
void	uaudio_chan_init
	(struct chan *, int, int, const struct audio_params *);
void	uaudio_chan_set_param(struct chan *, u_char *, u_char *, int);
void	uaudio_chan_ptransfer(struct chan *);
void	uaudio_chan_pintr
	(struct usbd_xfer *, void *, usbd_status);
void	uaudio_chan_psync_transfer(struct chan *);
void	uaudio_chan_psync_intr
	(struct usbd_xfer *, void *, usbd_status);

void	uaudio_chan_rtransfer(struct chan *);
void	uaudio_chan_rintr
	(struct usbd_xfer *, void *, usbd_status);

int	uaudio_open(void *, int);
void	uaudio_close(void *);
int	uaudio_drain(void *);
int	uaudio_query_encoding(void *, struct audio_encoding *);
void	uaudio_get_minmax_rates
	(int, const struct as_info *, const struct audio_params *,
	 int, int, int, u_long *, u_long *);
int	uaudio_match_alt_rate(void *, int, int);
int	uaudio_match_alt(void *, struct audio_params *, int);
int	uaudio_set_params
	(void *, int, int, struct audio_params *, struct audio_params *);
int	uaudio_round_blocksize(void *, int);
int	uaudio_trigger_output
	(void *, void *, void *, int, void (*)(void *), void *,
	 struct audio_params *);
int	uaudio_trigger_input
	(void *, void *, void *, int, void (*)(void *), void *,
	 struct audio_params *);
int	uaudio_halt_in_dma(void *);
int	uaudio_halt_out_dma(void *);
int	uaudio_getdev(void *, struct audio_device *);
int	uaudio_mixer_set_port(void *, mixer_ctrl_t *);
int	uaudio_mixer_get_port(void *, mixer_ctrl_t *);
int	uaudio_query_devinfo(void *, mixer_devinfo_t *);
int	uaudio_get_props(void *);
void	uaudio_get_default_params(void *, int, struct audio_params *);

struct audio_hw_if uaudio_hw_if = {
	uaudio_open,
	uaudio_close,
	uaudio_drain,
	uaudio_query_encoding,
	uaudio_set_params,
	uaudio_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_halt_out_dma,
	uaudio_halt_in_dma,
	NULL,
	uaudio_getdev,
	NULL,
	uaudio_mixer_set_port,
	uaudio_mixer_get_port,
	uaudio_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_get_props,
	uaudio_trigger_output,
	uaudio_trigger_input,
	uaudio_get_default_params
};

struct audio_device uaudio_device = {
	"USB audio",
	"",
	"uaudio"
};

int uaudio_match(struct device *, void *, void *); 
void uaudio_attach(struct device *, struct device *, void *); 
int uaudio_detach(struct device *, int); 

struct cfdriver uaudio_cd = { 
	NULL, "uaudio", DV_DULL 
}; 

const struct cfattach uaudio_ca = {
	sizeof(struct uaudio_softc), uaudio_match, uaudio_attach, uaudio_detach
};

int
uaudio_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	const usb_interface_descriptor_t *cd_id;
	usb_config_descriptor_t *cdesc;
	struct uaudio_devs *quirk;
	const char *buf;
	int flags = 0, size, offs;

	if (uaa->iface == NULL || uaa->device == NULL)
		return (UMATCH_NONE);

	quirk = uaudio_lookup(uaa->vendor, uaa->product);
	if (quirk)
		flags = quirk->flags;

	if (flags & UAUDIO_FLAG_BAD_AUDIO)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return (UMATCH_NONE);

	if (!(id->bInterfaceClass == UICLASS_AUDIO ||
	    ((flags & UAUDIO_FLAG_VENDOR_CLASS) &&
	    id->bInterfaceClass == UICLASS_VENDOR)))
		return (UMATCH_NONE);

	if (id->bInterfaceSubClass != UISUBCLASS_AUDIOCONTROL)
		return (UMATCH_NONE);

	cdesc = usbd_get_config_descriptor(uaa->device);
	if (cdesc == NULL)
		return (UMATCH_NONE);

	size = UGETW(cdesc->wTotalLength);
	buf = (const char *)cdesc;

	offs = 0;
	cd_id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOSTREAM,
	    flags);
	if (cd_id == NULL)
		return (UMATCH_NONE);

	offs = 0;
	cd_id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOCONTROL,
	    flags);
	if (cd_id == NULL)
		return (UMATCH_NONE);

	return (UMATCH_VENDOR_PRODUCT_CONF_IFACE);
}

void
uaudio_attach(struct device *parent, struct device *self, void *aux)
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uaudio_devs *quirk;
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t *cdesc;
	usbd_status err;
	int i, j, found;

	sc->sc_udev = uaa->device;

	quirk = uaudio_lookup(uaa->vendor, uaa->product);
	if (quirk)
		sc->sc_quirks = quirk->flags;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
		       sc->sc_dev.dv_xname);
		return;
	}

	err = uaudio_identify(sc, cdesc);
	if (err) {
		printf("%s: audio descriptors make no sense, error=%d\n",
		       sc->sc_dev.dv_xname, err);
		return;
	}

	/* Pick up the AS interface. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (usbd_iface_claimed(sc->sc_udev, i))
			continue;
		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id == NULL)
			continue;
		found = 0;
		for (j = 0; j < sc->sc_nalts; j++) {
			if (id->bInterfaceNumber ==
			    sc->sc_alts[j].idesc->bInterfaceNumber) {
				sc->sc_alts[j].ifaceh = uaa->ifaces[i];
				found = 1;
			}
		}
		if (found)
			usbd_claim_iface(sc->sc_udev, i);
	}

	for (j = 0; j < sc->sc_nalts; j++) {
		if (sc->sc_alts[j].ifaceh == NULL) {
			printf("%s: alt %d missing AS interface(s)\n",
			    sc->sc_dev.dv_xname, j);
			return;
		}
	}

	printf("%s: audio rev %d.%02x", sc->sc_dev.dv_xname,
	       sc->sc_audio_rev >> 8, sc->sc_audio_rev & 0xff);

	sc->sc_playchan.sc = sc->sc_recchan.sc = sc;
	sc->sc_playchan.altidx = -1;
	sc->sc_recchan.altidx = -1;

	if (sc->sc_quirks & UAUDIO_FLAG_NO_FRAC)
		sc->sc_altflags |= UA_NOFRAC;

	printf(", %d mixer controls\n", sc->sc_nctls);

	uaudio_create_encodings(sc);

	DPRINTF(("uaudio_attach: doing audio_attach_mi\n"));
	audio_attach_mi(&uaudio_hw_if, sc, &sc->sc_dev);
}

int
uaudio_detach(struct device *self, int flags)
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	int rv = 0;

	/*
	 * sc_alts may be NULL if uaudio_identify_as() failed, in
	 * which case uaudio_attach() didn't finish and there's
	 * nothing to detach.
	 */
	if (sc->sc_alts == NULL)
		return (rv);

	/* Wait for outstanding requests to complete. */
	uaudio_drain(sc);

	rv = config_detach_children(self, flags);

	return (rv);
}

int
uaudio_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct uaudio_softc *sc = addr;

	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	if (sc->sc_nalts == 0 || sc->sc_altflags == 0)
		return (ENXIO);

	if (fp->index < 0 || fp->index >= sc->sc_nencs)
		return (EINVAL);

	*fp = sc->sc_encs[fp->index];

	return (0);
}

const usb_interface_descriptor_t *
uaudio_find_iface(const char *buf, int size, int *offsp, int subtype, int flags)
{
	const usb_interface_descriptor_t *d;

	while (*offsp < size) {
		d = (const void *)(buf + *offsp);
		*offsp += d->bLength;
		if (d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceSubClass == subtype &&
		    (d->bInterfaceClass == UICLASS_AUDIO ||
		    (d->bInterfaceClass == UICLASS_VENDOR &&
		    (flags & UAUDIO_FLAG_VENDOR_CLASS))))
			return (d);
	}
	return (NULL);
}

void
uaudio_mixer_add_ctl(struct uaudio_softc *sc, struct mixerctl *mc)
{
	int res, range;
	size_t len;
	struct mixerctl *nmc;

	if (mc->class < UAC_NCLASSES) {
		DPRINTF(("%s: adding %s.%s\n",
			 __func__, uac_names[mc->class], mc->ctlname));
	} else {
		DPRINTF(("%s: adding %s\n", __func__, mc->ctlname));
	}

	nmc = mallocarray(sc->sc_nctls + 1, sizeof(*mc), M_USBDEV, M_NOWAIT);
	if (nmc == NULL) {
		printf("uaudio_mixer_add_ctl: no memory\n");
		return;
	}
	len = sizeof(*mc) * (sc->sc_nctls + 1);

	/* Copy old data, if there was any */
	if (sc->sc_nctls != 0) {
		bcopy(sc->sc_ctls, nmc, sizeof(*mc) * (sc->sc_nctls));
		free(sc->sc_ctls, M_USBDEV, 0);
	}
	sc->sc_ctls = nmc;

	mc->delta = 0;
	if (mc->type == MIX_ON_OFF) {
		mc->minval = 0;
		mc->maxval = 1;
	} else if (mc->type == MIX_SELECTOR) {
		;
	} else {
		/* Determine min and max values. */
		mc->minval = uaudio_signext(mc->type,
			uaudio_get(sc, GET_MIN, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
		mc->maxval = uaudio_signext(mc->type,
			uaudio_get(sc, GET_MAX, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
		range = mc->maxval - mc->minval;
		res = uaudio_get(sc, GET_RES, UT_READ_CLASS_INTERFACE,
				 mc->wValue[0], mc->wIndex,
				 MIX_SIZE(mc->type));
		if (res > 0 && range > 0)
			mc->delta = (res * 255 + res - 1) / range;
	}

	sc->sc_ctls[sc->sc_nctls++] = *mc;

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 2) {
		int i;
		DPRINTF(("uaudio_mixer_add_ctl: wValue=%04x",mc->wValue[0]));
		for (i = 1; i < mc->nchan; i++)
			DPRINTF((",%04x", mc->wValue[i]));
		DPRINTF((" wIndex=%04x type=%d name='%s' unit='%s' "
			 "min=%d max=%d\n",
			 mc->wIndex, mc->type, mc->ctlname, mc->ctlunit,
			 mc->minval, mc->maxval));
	}
#endif
}

char *
uaudio_id_name(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	static char buf[32];
	snprintf(buf, sizeof(buf), "i%d", id);
	return (buf);
}

uByte
uaudio_get_cluster_nchan(int id, const struct io_terminal *iot)
{
	struct usb_audio_cluster r;
	const usb_descriptor_t *dp;
	int i;

	for (i = 0; i < 25; i++) { /* avoid infinite loops */
		dp = iot[id].d.desc;
		if (dp == 0)
			goto bad;
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			return (iot[id].d.it->bNrChannels);
		case UDESCSUB_AC_OUTPUT:
			id = iot[id].d.ot->bSourceId;
			break;
		case UDESCSUB_AC_MIXER:
			r = *(struct usb_audio_cluster *)
				&iot[id].d.mu->baSourceId[iot[id].d.mu->bNrInPins];
			return (r.bNrChannels);
		case UDESCSUB_AC_SELECTOR:
			/* XXX This is not really right */
			id = iot[id].d.su->baSourceId[0];
			break;
		case UDESCSUB_AC_FEATURE:
			id = iot[id].d.fu->bSourceId;
			break;
		case UDESCSUB_AC_PROCESSING:
			r = *(struct usb_audio_cluster *)
				&iot[id].d.pu->baSourceId[iot[id].d.pu->bNrInPins];
			return (r.bNrChannels);
		case UDESCSUB_AC_EXTENSION:
			r = *(struct usb_audio_cluster *)
				&iot[id].d.eu->baSourceId[iot[id].d.eu->bNrInPins];
			return (r.bNrChannels);
		default:
			goto bad;
		}
	}
bad:
	printf("uaudio_get_cluster_nchan: bad data\n");
	return (0);
}

void
uaudio_add_input(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
#ifdef UAUDIO_DEBUG
	const struct usb_audio_input_terminal *d = iot[id].d.it;

	DPRINTFN(2,("uaudio_add_input: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bNrChannels=%d wChannelConfig=%d "
		    "iChannelNames=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bNrChannels, UGETW(d->wChannelConfig),
		    d->iChannelNames, d->iTerminal));
#endif
}

void
uaudio_add_output(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
#ifdef UAUDIO_DEBUG
	const struct usb_audio_output_terminal *d = iot[id].d.ot;

	DPRINTFN(2,("uaudio_add_output: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bSourceId=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bSourceId, d->iTerminal));
#endif
}

void
uaudio_add_mixer(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_mixer_unit *d = iot[id].d.mu;
	struct usb_audio_mixer_unit_1 *d1;
	int c, chs, ichs, ochs, i, o, bno, p, mo, mc, k;
	uByte *bm;
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_mixer: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));

	/* Compute the number of input channels */
	ichs = 0;
	for (i = 0; i < d->bNrInPins; i++)
		ichs += uaudio_get_cluster_nchan(d->baSourceId[i], iot);

	/* and the number of output channels */
	d1 = (struct usb_audio_mixer_unit_1 *)&d->baSourceId[d->bNrInPins];
	ochs = d1->bNrChannels;
	DPRINTFN(2,("uaudio_add_mixer: ichs=%d ochs=%d\n", ichs, ochs));

	bm = d1->bmControls;
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	uaudio_determine_class(&iot[id], &mix);
	mix.type = MIX_SIGNED_16;
	mix.ctlunit = AudioNvolume;
#define BIT(bno) ((bm[bno / 8] >> (7 - bno % 8)) & 1)
	for (p = i = 0; i < d->bNrInPins; i++) {
		chs = uaudio_get_cluster_nchan(d->baSourceId[i], iot);
		mc = 0;
		for (c = 0; c < chs; c++) {
			mo = 0;
			for (o = 0; o < ochs; o++) {
				bno = (p + c) * ochs + o;
				if (BIT(bno))
					mo++;
			}
			if (mo == 1)
				mc++;
		}
		if (mc == chs && chs <= MIX_MAX_CHAN) {
			k = 0;
			for (c = 0; c < chs; c++)
				for (o = 0; o < ochs; o++) {
					bno = (p + c) * ochs + o;
					if (BIT(bno))
						mix.wValue[k++] =
							MAKE(p+c+1, o+1);
				}
			snprintf(mix.ctlname, sizeof(mix.ctlname), "mix%d-%s",
			    d->bUnitId, uaudio_id_name(sc, iot,
			    d->baSourceId[i]));
			mix.nchan = chs;
			uaudio_mixer_add_ctl(sc, &mix);
		} else {
			/* XXX */
		}
#undef BIT
		p += chs;
	}

}

void
uaudio_add_selector(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_selector_unit *d = iot[id].d.su;
	struct mixerctl mix;
	int i, wp;

	DPRINTFN(2,("uaudio_add_selector: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.wValue[0] = MAKE(0, 0);
	uaudio_determine_class(&iot[id], &mix);
	mix.nchan = 1;
	mix.type = MIX_SELECTOR;
	mix.ctlunit = "";
	mix.minval = 1;
	mix.maxval = d->bNrInPins;
	wp = snprintf(mix.ctlname, MAX_AUDIO_DEV_LEN, "sel%d-", d->bUnitId);
	for (i = 1; i <= d->bNrInPins; i++) {
		wp += snprintf(mix.ctlname + wp, MAX_AUDIO_DEV_LEN - wp,
			       "i%d", d->baSourceId[i - 1]);
		if (wp > MAX_AUDIO_DEV_LEN - 1)
			break;
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

#ifdef UAUDIO_DEBUG
const char *
uaudio_get_terminal_name(int terminal_type)
{
	static char buf[100];

	switch (terminal_type) {
	/* USB terminal types */
	case UAT_UNDEFINED:	return "UAT_UNDEFINED";
	case UAT_STREAM:	return "UAT_STREAM";
	case UAT_VENDOR:	return "UAT_VENDOR";
	/* input terminal types */
	case UATI_UNDEFINED:	return "UATI_UNDEFINED";
	case UATI_MICROPHONE:	return "UATI_MICROPHONE";
	case UATI_DESKMICROPHONE:	return "UATI_DESKMICROPHONE";
	case UATI_PERSONALMICROPHONE:	return "UATI_PERSONALMICROPHONE";
	case UATI_OMNIMICROPHONE:	return "UATI_OMNIMICROPHONE";
	case UATI_MICROPHONEARRAY:	return "UATI_MICROPHONEARRAY";
	case UATI_PROCMICROPHONEARR:	return "UATI_PROCMICROPHONEARR";
	/* output terminal types */
	case UATO_UNDEFINED:	return "UATO_UNDEFINED";
	case UATO_SPEAKER:	return "UATO_SPEAKER";
	case UATO_HEADPHONES:	return "UATO_HEADPHONES";
	case UATO_DISPLAYAUDIO:	return "UATO_DISPLAYAUDIO";
	case UATO_DESKTOPSPEAKER:	return "UATO_DESKTOPSPEAKER";
	case UATO_ROOMSPEAKER:	return "UATO_ROOMSPEAKER";
	case UATO_COMMSPEAKER:	return "UATO_COMMSPEAKER";
	case UATO_SUBWOOFER:	return "UATO_SUBWOOFER";
	/* bidir terminal types */
	case UATB_UNDEFINED:	return "UATB_UNDEFINED";
	case UATB_HANDSET:	return "UATB_HANDSET";
	case UATB_HEADSET:	return "UATB_HEADSET";
	case UATB_SPEAKERPHONE:	return "UATB_SPEAKERPHONE";
	case UATB_SPEAKERPHONEESUP:	return "UATB_SPEAKERPHONEESUP";
	case UATB_SPEAKERPHONEECANC:	return "UATB_SPEAKERPHONEECANC";
	/* telephony terminal types */
	case UATT_UNDEFINED:	return "UATT_UNDEFINED";
	case UATT_PHONELINE:	return "UATT_PHONELINE";
	case UATT_TELEPHONE:	return "UATT_TELEPHONE";
	case UATT_DOWNLINEPHONE:	return "UATT_DOWNLINEPHONE";
	/* external terminal types */
	case UATE_UNDEFINED:	return "UATE_UNDEFINED";
	case UATE_ANALOGCONN:	return "UATE_ANALOGCONN";
	case UATE_LINECONN:	return "UATE_LINECONN";
	case UATE_LEGACYCONN:	return "UATE_LEGACYCONN";
	case UATE_DIGITALAUIFC:	return "UATE_DIGITALAUIFC";
	case UATE_SPDIF:	return "UATE_SPDIF";
	case UATE_1394DA:	return "UATE_1394DA";
	case UATE_1394DV:	return "UATE_1394DV";
	/* embedded function terminal types */
	case UATF_UNDEFINED:	return "UATF_UNDEFINED";
	case UATF_CALIBNOISE:	return "UATF_CALIBNOISE";
	case UATF_EQUNOISE:	return "UATF_EQUNOISE";
	case UATF_CDPLAYER:	return "UATF_CDPLAYER";
	case UATF_DAT:	return "UATF_DAT";
	case UATF_DCC:	return "UATF_DCC";
	case UATF_MINIDISK:	return "UATF_MINIDISK";
	case UATF_ANALOGTAPE:	return "UATF_ANALOGTAPE";
	case UATF_PHONOGRAPH:	return "UATF_PHONOGRAPH";
	case UATF_VCRAUDIO:	return "UATF_VCRAUDIO";
	case UATF_VIDEODISCAUDIO:	return "UATF_VIDEODISCAUDIO";
	case UATF_DVDAUDIO:	return "UATF_DVDAUDIO";
	case UATF_TVTUNERAUDIO:	return "UATF_TVTUNERAUDIO";
	case UATF_SATELLITE:	return "UATF_SATELLITE";
	case UATF_CABLETUNER:	return "UATF_CABLETUNER";
	case UATF_DSS:	return "UATF_DSS";
	case UATF_RADIORECV:	return "UATF_RADIORECV";
	case UATF_RADIOXMIT:	return "UATF_RADIOXMIT";
	case UATF_MULTITRACK:	return "UATF_MULTITRACK";
	case UATF_SYNTHESIZER:	return "UATF_SYNTHESIZER";
	default:
		snprintf(buf, sizeof(buf), "unknown type (0x%.4x)", terminal_type);
		return buf;
	}
}
#endif

int
uaudio_determine_class(const struct io_terminal *iot, struct mixerctl *mix)
{
	int terminal_type;

	if (iot == NULL || iot->output == NULL) {
		mix->class = UAC_OUTPUT;
		return 0;
	}
	terminal_type = 0;
	if (iot->output->size == 1)
		terminal_type = iot->output->terminals[0];
	/*
	 * If the only output terminal is USB,
	 * the class is UAC_RECORD.
	 */
	if ((terminal_type & 0xff00) == (UAT_UNDEFINED & 0xff00)) {
		mix->class = UAC_RECORD;
		if (iot->inputs_size == 1
		    && iot->inputs[0] != NULL
		    && iot->inputs[0]->size == 1)
			return iot->inputs[0]->terminals[0];
		else
			return 0;
	}
	/*
	 * If the ultimate destination of the unit is just one output
	 * terminal and the unit is connected to the output terminal
	 * directly, the class is UAC_OUTPUT.
	 */
	if (terminal_type != 0 && iot->direct) {
		mix->class = UAC_OUTPUT;
		return terminal_type;
	}
	/*
	 * If the unit is connected to just one input terminal,
	 * the class is UAC_INPUT.
	 */
	if (iot->inputs_size == 1 && iot->inputs[0] != NULL
	    && iot->inputs[0]->size == 1) {
		mix->class = UAC_INPUT;
		return iot->inputs[0]->terminals[0];
	}
	/*
	 * Otherwise, the class is UAC_OUTPUT.
	 */
	mix->class = UAC_OUTPUT;
	return terminal_type;
}

const char *
uaudio_feature_name(const struct io_terminal *iot, struct mixerctl *mix)
{
	int terminal_type;

	terminal_type = uaudio_determine_class(iot, mix);
	if (mix->class == UAC_RECORD && terminal_type == 0)
		return AudioNmixerout;
	DPRINTF(("%s: terminal_type=%s\n", __func__,
		 uaudio_get_terminal_name(terminal_type)));
	switch (terminal_type) {
	case UAT_STREAM:
		return AudioNdac;

	case UATI_MICROPHONE:
	case UATI_DESKMICROPHONE:
	case UATI_PERSONALMICROPHONE:
	case UATI_OMNIMICROPHONE:
	case UATI_MICROPHONEARRAY:
	case UATI_PROCMICROPHONEARR:
		return AudioNmicrophone;

	case UATO_SPEAKER:
	case UATO_DESKTOPSPEAKER:
	case UATO_ROOMSPEAKER:
	case UATO_COMMSPEAKER:
		return AudioNspeaker;

	case UATO_HEADPHONES:
		return AudioNheadphone;

	case UATO_SUBWOOFER:
		return AudioNlfe;

	/* telephony terminal types */
	case UATT_UNDEFINED:
	case UATT_PHONELINE:
	case UATT_TELEPHONE:
	case UATT_DOWNLINEPHONE:
		return "phone";

	case UATE_ANALOGCONN:
	case UATE_LINECONN:
	case UATE_LEGACYCONN:
		return AudioNline;

	case UATE_DIGITALAUIFC:
	case UATE_SPDIF:
	case UATE_1394DA:
	case UATE_1394DV:
		return AudioNaux;

	case UATF_CDPLAYER:
		return AudioNcd;

	case UATF_SYNTHESIZER:
		return AudioNfmsynth;

	case UATF_VIDEODISCAUDIO:
	case UATF_DVDAUDIO:
	case UATF_TVTUNERAUDIO:
		return AudioNvideo;

	case UAT_UNDEFINED:
	case UAT_VENDOR:
	case UATI_UNDEFINED:
/* output terminal types */
	case UATO_UNDEFINED:
	case UATO_DISPLAYAUDIO:
/* bidir terminal types */
	case UATB_UNDEFINED:
	case UATB_HANDSET:
	case UATB_HEADSET:
	case UATB_SPEAKERPHONE:
	case UATB_SPEAKERPHONEESUP:
	case UATB_SPEAKERPHONEECANC:
/* external terminal types */
	case UATE_UNDEFINED:
/* embedded function terminal types */
	case UATF_UNDEFINED:
	case UATF_CALIBNOISE:
	case UATF_EQUNOISE:
	case UATF_DAT:
	case UATF_DCC:
	case UATF_MINIDISK:
	case UATF_ANALOGTAPE:
	case UATF_PHONOGRAPH:
	case UATF_VCRAUDIO:
	case UATF_SATELLITE:
	case UATF_CABLETUNER:
	case UATF_DSS:
	case UATF_RADIORECV:
	case UATF_RADIOXMIT:
	case UATF_MULTITRACK:
	case 0xffff:
	default:
		DPRINTF(("%s: 'master' for 0x%.4x\n", __func__, terminal_type));
		return AudioNmaster;
	}
}

void
uaudio_add_feature(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_feature_unit *d = iot[id].d.fu;
	uByte *ctls = (uByte *)d->bmaControls;
	int ctlsize = d->bControlSize;
	int nchan = (d->bLength - 7) / ctlsize;
	u_int fumask, mmask, cmask;
	struct mixerctl mix;
	int chan, ctl, i, unit;
	const char *mixername;

#define GET(i) (ctls[(i)*ctlsize] | \
		(ctlsize > 1 ? ctls[(i)*ctlsize+1] << 8 : 0))

	mmask = GET(0);
	/* Figure out what we can control */
	for (cmask = 0, chan = 1; chan < nchan; chan++) {
		DPRINTFN(9,("uaudio_add_feature: chan=%d mask=%x\n",
			    chan, GET(chan)));
		cmask |= GET(chan);
	}

	DPRINTFN(1,("uaudio_add_feature: bUnitId=%d, "
		    "%d channels, mmask=0x%04x, cmask=0x%04x\n",
		    d->bUnitId, nchan, mmask, cmask));

	if (nchan > MIX_MAX_CHAN)
		nchan = MIX_MAX_CHAN;
	unit = d->bUnitId;
	mix.wIndex = MAKE(unit, sc->sc_ac_iface);
	for (ctl = MUTE_CONTROL; ctl < LOUDNESS_CONTROL; ctl++) {
		fumask = FU_MASK(ctl);
		DPRINTFN(4,("uaudio_add_feature: ctl=%d fumask=0x%04x\n",
			    ctl, fumask));
		if (mmask & fumask) {
			mix.nchan = 1;
			mix.wValue[0] = MAKE(ctl, 0);
		} else if (cmask & fumask) {
			mix.nchan = nchan - 1;
			for (i = 1; i < nchan; i++) {
				if (GET(i) & fumask)
					mix.wValue[i-1] = MAKE(ctl, i);
				else
					mix.wValue[i-1] = -1;
			}
		} else {
			continue;
		}
#undef GET
		mixername = uaudio_feature_name(&iot[id], &mix);
		switch (ctl) {
		case MUTE_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNmute);
			break;
		case VOLUME_CONTROL:
			mix.type = MIX_SIGNED_16;
			mix.ctlunit = AudioNvolume;
			strlcpy(mix.ctlname, mixername, sizeof(mix.ctlname));
			break;
		case BASS_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNbass;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNbass);
			break;
		case MID_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNmid;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNmid);
			break;
		case TREBLE_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctlunit = AudioNtreble;
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNtreble);
			break;
		case GRAPHIC_EQUALIZER_CONTROL:
			continue; /* XXX don't add anything */
			break;
		case AGC_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname), "%s.%s",
				 mixername, AudioNagc);
			break;
		case DELAY_CONTROL:
			mix.type = MIX_UNSIGNED_16;
			mix.ctlunit = "4 ms";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNdelay);
			break;
		case BASS_BOOST_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNbassboost);
			break;
		case LOUDNESS_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctlunit = "";
			snprintf(mix.ctlname, sizeof(mix.ctlname),
				 "%s.%s", mixername, AudioNloudness);
			break;
		}
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

void
uaudio_add_processing_updown(struct uaudio_softc *sc,
			     const struct io_terminal *iot, int id)
{
	const struct usb_audio_processing_unit *d = iot[id].d.pu;
	const struct usb_audio_processing_unit_1 *d1 =
	    (const struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	const struct usb_audio_processing_unit_updown *ud =
	    (const struct usb_audio_processing_unit_updown *)
		&d1->bmControls[d1->bControlSize];
	struct mixerctl mix;
	int i;

	DPRINTFN(2,("uaudio_add_processing_updown: bUnitId=%d bNrModes=%d\n",
		    d->bUnitId, ud->bNrModes));

	if (!(d1->bmControls[0] & UA_PROC_MASK(UD_MODE_SELECT_CONTROL))) {
		DPRINTF(("uaudio_add_processing_updown: no mode select\n"));
		return;
	}

	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.nchan = 1;
	mix.wValue[0] = MAKE(UD_MODE_SELECT_CONTROL, 0);
	uaudio_determine_class(&iot[id], &mix);
	mix.type = MIX_ON_OFF;	/* XXX */
	mix.ctlunit = "";
	snprintf(mix.ctlname, sizeof(mix.ctlname), "pro%d-mode", d->bUnitId);

	for (i = 0; i < ud->bNrModes; i++) {
		DPRINTFN(2,("uaudio_add_processing_updown: i=%d bm=0x%x\n",
			    i, UGETW(ud->waModes[i])));
		/* XXX */
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

void
uaudio_add_processing(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_processing_unit *d = iot[id].d.pu;
	const struct usb_audio_processing_unit_1 *d1 =
	    (const struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	int ptype = UGETW(d->wProcessType);
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_processing: wProcessType=%d bUnitId=%d "
		    "bNrInPins=%d\n", ptype, d->bUnitId, d->bNrInPins));

	if (d1->bmControls[0] & UA_PROC_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(XX_ENABLE_CONTROL, 0);
		uaudio_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		snprintf(mix.ctlname, sizeof(mix.ctlname), "pro%d.%d-enable",
		    d->bUnitId, ptype);
		uaudio_mixer_add_ctl(sc, &mix);
	}

	switch(ptype) {
	case UPDOWNMIX_PROCESS:
		uaudio_add_processing_updown(sc, iot, id);
		break;
	case DOLBY_PROLOGIC_PROCESS:
	case P3D_STEREO_EXTENDER_PROCESS:
	case REVERBATION_PROCESS:
	case CHORUS_PROCESS:
	case DYN_RANGE_COMP_PROCESS:
	default:
#ifdef UAUDIO_DEBUG
		printf("uaudio_add_processing: unit %d, type=%d not impl.\n",
		       d->bUnitId, ptype);
#endif
		break;
	}
}

void
uaudio_add_extension(struct uaudio_softc *sc, const struct io_terminal *iot, int id)
{
	const struct usb_audio_extension_unit *d = iot[id].d.eu;
	const struct usb_audio_extension_unit_1 *d1 =
	    (const struct usb_audio_extension_unit_1 *)&d->baSourceId[d->bNrInPins];
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_extension: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));

	if (sc->sc_quirks & UAUDIO_FLAG_NO_XU)
		return;

	if (d1->bmControls[0] & UA_EXT_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(UA_EXT_ENABLE, 0);
		uaudio_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		snprintf(mix.ctlname, sizeof(mix.ctlname), "ext%d-enable",
		    d->bUnitId);
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

struct terminal_list*
uaudio_merge_terminal_list(const struct io_terminal *iot)
{
	struct terminal_list *tml;
	uint16_t *ptm;
	int i, len;

	len = 0;
	if (iot->inputs == NULL)
		return NULL;
	for (i = 0; i < iot->inputs_size; i++) {
		if (iot->inputs[i] != NULL)
			len += iot->inputs[i]->size;
	}
	tml = malloc(TERMINAL_LIST_SIZE(len), M_TEMP, M_NOWAIT);
	if (tml == NULL) {
		printf("uaudio_merge_terminal_list: no memory\n");
		return NULL;
	}
	tml->size = 0;
	ptm = tml->terminals;
	for (i = 0; i < iot->inputs_size; i++) {
		if (iot->inputs[i] == NULL)
			continue;
		if (iot->inputs[i]->size > len)
			break;
		memcpy(ptm, iot->inputs[i]->terminals,
		       iot->inputs[i]->size * sizeof(uint16_t));
		tml->size += iot->inputs[i]->size;
		ptm += iot->inputs[i]->size;
		len -= iot->inputs[i]->size;
	}
	return tml;
}

struct terminal_list *
uaudio_io_terminaltype(int outtype, struct io_terminal *iot, int id)
{
	struct terminal_list *tml;
	struct io_terminal *it;
	int src_id, i;

	it = &iot[id];
	if (it->output != NULL) {
		/* already has outtype? */
		for (i = 0; i < it->output->size; i++)
			if (it->output->terminals[i] == outtype)
				return uaudio_merge_terminal_list(it);
		tml = malloc(TERMINAL_LIST_SIZE(it->output->size + 1),
			     M_TEMP, M_NOWAIT);
		if (tml == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return uaudio_merge_terminal_list(it);
		}
		memcpy(tml, it->output, TERMINAL_LIST_SIZE(it->output->size));
		tml->terminals[it->output->size] = outtype;
		tml->size++;
		free(it->output, M_TEMP, 0);
		it->output = tml;
		if (it->inputs != NULL) {
			for (i = 0; i < it->inputs_size; i++)
				if (it->inputs[i] != NULL)
					free(it->inputs[i], M_TEMP, 0);
			free(it->inputs, M_TEMP, 0);
		}
		it->inputs_size = 0;
		it->inputs = NULL;
	} else {		/* end `iot[id] != NULL' */
		it->inputs_size = 0;
		it->inputs = NULL;
		it->output = malloc(TERMINAL_LIST_SIZE(1), M_TEMP, M_NOWAIT);
		if (it->output == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		it->output->terminals[0] = outtype;
		it->output->size = 1;
		it->direct = 0;
	}

	switch (it->d.desc->bDescriptorSubtype) {
	case UDESCSUB_AC_INPUT:
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		tml = malloc(TERMINAL_LIST_SIZE(1), M_TEMP, M_NOWAIT);
		if (tml == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			free(it->inputs, M_TEMP, 0);
			it->inputs = NULL;
			return NULL;
		}
		it->inputs[0] = tml;
		tml->terminals[0] = UGETW(it->d.it->wTerminalType);
		tml->size = 1;
		it->inputs_size = 1;
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_FEATURE:
		src_id = it->d.fu->bSourceId;
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return uaudio_io_terminaltype(outtype, iot, src_id);
		}
		it->inputs[0] = uaudio_io_terminaltype(outtype, iot, src_id);
		it->inputs_size = 1;
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_OUTPUT:
		it->inputs = malloc(sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		src_id = it->d.ot->bSourceId;
		it->inputs[0] = uaudio_io_terminaltype(outtype, iot, src_id);
		it->inputs_size = 1;
		iot[src_id].direct = 1;
		return NULL;
	case UDESCSUB_AC_MIXER:
		it->inputs_size = 0;
		it->inputs = mallocarray(it->d.mu->bNrInPins,
		    sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.mu->bNrInPins; i++) {
			src_id = it->d.mu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_SELECTOR:
		it->inputs_size = 0;
		it->inputs = mallocarray(it->d.su->bNrInPins,
		    sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.su->bNrInPins; i++) {
			src_id = it->d.su->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_PROCESSING:
		it->inputs_size = 0;
		it->inputs = mallocarray(it->d.pu->bNrInPins,
		    sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.pu->bNrInPins; i++) {
			src_id = it->d.pu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_EXTENSION:
		it->inputs_size = 0;
		it->inputs = mallocarray(it->d.eu->bNrInPins,
		    sizeof(struct terminal_list *), M_TEMP, M_NOWAIT);
		if (it->inputs == NULL) {
			printf("uaudio_io_terminaltype: no memory\n");
			return NULL;
		}
		for (i = 0; i < it->d.eu->bNrInPins; i++) {
			src_id = it->d.eu->baSourceId[i];
			it->inputs[i] = uaudio_io_terminaltype(outtype, iot,
							       src_id);
			it->inputs_size++;
		}
		return uaudio_merge_terminal_list(it);
	case UDESCSUB_AC_HEADER:
	default:
		return NULL;
	}
}

usbd_status
uaudio_identify(struct uaudio_softc *sc, const usb_config_descriptor_t *cdesc)
{
	usbd_status err;

	err = uaudio_identify_ac(sc, cdesc);
	if (err)
		return (err);
	return (uaudio_identify_as(sc, cdesc));
}

void
uaudio_add_alt(struct uaudio_softc *sc, const struct as_info *ai)
{
	size_t len;
	struct as_info *nai;

	nai = mallocarray(sc->sc_nalts + 1, sizeof(*ai), M_USBDEV, M_NOWAIT);
	if (nai == NULL) {
		printf("uaudio_add_alt: no memory\n");
		return;
	}
	len = sizeof(*ai) * (sc->sc_nalts + 1);

	/* Copy old data, if there was any */
	if (sc->sc_nalts != 0) {
		bcopy(sc->sc_alts, nai, sizeof(*ai) * (sc->sc_nalts));
		free(sc->sc_alts, M_USBDEV, 0);
	}
	sc->sc_alts = nai;
	DPRINTFN(2,("uaudio_add_alt: adding alt=%d, enc=%d\n",
		    ai->alt, ai->encoding));
	sc->sc_alts[sc->sc_nalts++] = *ai;
}

void
uaudio_create_encodings(struct uaudio_softc *sc)
{
	int enc, encs[16], nencs, i, j;

	nencs = 0;
	for (i = 0; i < sc->sc_nalts && nencs < 16; i++) {
		enc = (sc->sc_alts[i].asf1desc->bSubFrameSize << 16) |
		    (sc->sc_alts[i].asf1desc->bBitResolution << 8) |
		    sc->sc_alts[i].encoding;
		for (j = 0; j < nencs; j++) {
			if (encs[j] == enc)
				break;
		}
		if (j < nencs)
			continue;
		encs[j] = enc;
		nencs++;
	}

	sc->sc_nencs = 0;
	sc->sc_encs = mallocarray(nencs, sizeof(struct audio_encoding),
	    M_USBDEV, M_NOWAIT);
	if (sc->sc_encs == NULL) {
		printf("%s: no memory\n", __func__);
		return;
	}
	sc->sc_nencs = nencs;

	for (i = 0; i < sc->sc_nencs; i++) {
		sc->sc_encs[i].index = i;
		sc->sc_encs[i].encoding = encs[i] & 0xff;
		sc->sc_encs[i].precision = (encs[i] >> 8) & 0xff;
		sc->sc_encs[i].bps = (encs[i] >> 16) & 0xff;
		sc->sc_encs[i].msb = 1;
		sc->sc_encs[i].flags = 0;
		switch (sc->sc_encs[i].encoding) {
		case AUDIO_ENCODING_SLINEAR_LE:
			strlcpy(sc->sc_encs[i].name,
			    sc->sc_encs[i].precision == 8 ?
			    AudioEslinear : AudioEslinear_le,
			    sizeof(sc->sc_encs[i].name));
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (sc->sc_encs[i].precision != 8) {
				DPRINTF(("%s: invalid precision for ulinear: %d\n",
				    __func__, sc->sc_encs[i].precision));
				continue;
			}
			strlcpy(sc->sc_encs[i].name, AudioEulinear,
			    sizeof(sc->sc_encs[i].name));
			break;
		case AUDIO_ENCODING_ALAW:
			if (sc->sc_encs[i].precision != 8) {
				DPRINTF(("%s: invalid precision for alaw: %d\n",
				    __func__, sc->sc_encs[i].precision));
				continue;
			}
			strlcpy(sc->sc_encs[i].name, AudioEalaw,
			    sizeof(sc->sc_encs[i].name));
			break;
		case AUDIO_ENCODING_ULAW:
			if (sc->sc_encs[i].precision != 8) {
				DPRINTF(("%s: invalid precision for ulaw: %d\n",
				    __func__, sc->sc_encs[i].precision));
				continue;
			}
			strlcpy(sc->sc_encs[i].name, AudioEmulaw,
			    sizeof(sc->sc_encs[i].name));
			break;
		default:
			DPRINTF(("%s: unknown format\n", __func__));
			break;
		}
	}
}

usbd_status
uaudio_process_as(struct uaudio_softc *sc, const char *buf, int *offsp,
		  int size, const usb_interface_descriptor_t *id)
#define offs (*offsp)
{
	const struct usb_audio_streaming_interface_descriptor *asid;
	const struct usb_audio_streaming_type1_descriptor *asf1d;
	const struct usb_endpoint_descriptor_audio *ed;
	const struct usb_endpoint_descriptor_audio *sync_ed;
	const struct usb_audio_streaming_endpoint_descriptor *sed;
	int format, chan, prec, enc, bps;
	int dir, type, sync, sync_addr;
	struct as_info ai;
	const char *format_str;

	asid = (const void *)(buf + offs);
	if (asid->bDescriptorType != UDESC_CS_INTERFACE ||
	    asid->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	DPRINTF(("uaudio_process_as: asid: bTerminalLink=%d wFormatTag=%d\n",
		 asid->bTerminalLink, UGETW(asid->wFormatTag)));
	offs += asid->bLength;
	if (offs > size)
		return (USBD_INVAL);

	asf1d = (const void *)(buf + offs);
	if (asf1d->bDescriptorType != UDESC_CS_INTERFACE ||
	    asf1d->bDescriptorSubtype != FORMAT_TYPE)
		return (USBD_INVAL);
	offs += asf1d->bLength;
	if (offs > size)
		return (USBD_INVAL);

	if (asf1d->bFormatType != FORMAT_TYPE_I) {
		printf("%s: ignored setting with type %d format\n",
		       sc->sc_dev.dv_xname, UGETW(asid->wFormatTag));
		return (USBD_NORMAL_COMPLETION);
	}

	ed = (const void *)(buf + offs);
	if (ed->bDescriptorType != UDESC_ENDPOINT)
		return (USBD_INVAL);
	DPRINTF(("uaudio_process_as: endpoint[0] bLength=%d bDescriptorType=%d "
		 "bEndpointAddress=%d bmAttributes=0x%x wMaxPacketSize=%d "
		 "bInterval=%d bRefresh=%d bSynchAddress=%d\n",
		 ed->bLength, ed->bDescriptorType, ed->bEndpointAddress,
		 ed->bmAttributes, UGETW(ed->wMaxPacketSize),
		 ed->bInterval, ed->bRefresh, ed->bSynchAddress));
	offs += ed->bLength;
	if (offs > size)
		return (USBD_INVAL);
	if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
		return (USBD_INVAL);

	dir = UE_GET_DIR(ed->bEndpointAddress);
	type = UE_GET_ISO_TYPE(ed->bmAttributes);

	/* Check for sync endpoint. */
	sync = 0;
	sync_addr = 0;
	if (id->bNumEndpoints > 1 &&
	    ((dir == UE_DIR_IN && type == UE_ISO_ADAPT) ||
	    (dir != UE_DIR_IN && type == UE_ISO_ASYNC)))
		sync = 1;

	/* Check whether sync endpoint address is given. */
	if (ed->bLength >= USB_ENDPOINT_DESCRIPTOR_AUDIO_SIZE) {
		/* bSynchAdress set to 0 indicates sync is not used. */
		if (ed->bSynchAddress == 0)
			sync = 0;
		else
			sync_addr = ed->bSynchAddress;
	}

	sed = (const void *)(buf + offs);
	if (sed->bDescriptorType != UDESC_CS_ENDPOINT ||
	    sed->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	DPRINTF((" streaming_endpoint: offset=%d bLength=%d\n", offs, sed->bLength));
	offs += sed->bLength;
	if (offs > size)
		return (USBD_INVAL);

	sync_ed = NULL;
	if (sync == 1) {
		sync_ed = (const void*)(buf + offs);
		if (sync_ed->bDescriptorType != UDESC_ENDPOINT) {
			printf("%s: sync ep descriptor wrong type\n",
			    sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
		DPRINTF(("uaudio_process_as: endpoint[1] bLength=%d "
			 "bDescriptorType=%d bEndpointAddress=%d "
			 "bmAttributes=0x%x wMaxPacketSize=%d bInterval=%d "
			 "bRefresh=%d bSynchAddress=%d\n",
			 sync_ed->bLength, sync_ed->bDescriptorType,
			 sync_ed->bEndpointAddress, sync_ed->bmAttributes,
			 UGETW(sync_ed->wMaxPacketSize), sync_ed->bInterval,
			 sync_ed->bRefresh, sync_ed->bSynchAddress));
		offs += sync_ed->bLength;
		if (offs > size) {
			printf("%s: sync ep descriptor too large\n",
			    sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
		if (dir == UE_GET_DIR(sync_ed->bEndpointAddress)) {
			printf("%s: sync ep wrong direction\n",
			       sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
		if (UE_GET_XFERTYPE(sync_ed->bmAttributes) != UE_ISOCHRONOUS) {
			printf("%s: sync ep wrong xfer type\n",
			       sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
		if (sync_ed->bLength >=
		    USB_ENDPOINT_DESCRIPTOR_AUDIO_SIZE &&
		    sync_ed->bSynchAddress != 0) {
			printf("%s: sync ep bSynchAddress != 0\n",
			       sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
		if (sync_addr &&
		    UE_GET_ADDR(sync_ed->bEndpointAddress) !=
		    UE_GET_ADDR(sync_addr)) {
			printf("%s: sync ep address mismatch\n",
			       sc->sc_dev.dv_xname);
			return (USBD_NORMAL_COMPLETION);
		}
	}
	if (sync_ed != NULL && dir == UE_DIR_IN) {
		printf("%s: sync pipe for recording not yet implemented\n",
		    sc->sc_dev.dv_xname);
		return (USBD_NORMAL_COMPLETION);
	}

	format = UGETW(asid->wFormatTag);
	chan = asf1d->bNrChannels;
	prec = asf1d->bBitResolution;
	bps = asf1d->bSubFrameSize;
	if ((prec != 8 && prec != 16 && prec != 24) || (bps < 1 || bps > 4)) {
		printf("%s: ignored setting with precision %d bps %d\n",
		       sc->sc_dev.dv_xname, prec, bps);
		return (USBD_NORMAL_COMPLETION);
	}
	switch (format) {
	case UA_FMT_PCM:
		if (prec == 8) {
			sc->sc_altflags |= HAS_8;
		} else if (prec == 16) {
			sc->sc_altflags |= HAS_16;
		} else if (prec == 24) {
			sc->sc_altflags |= HAS_24;
		}
		enc = AUDIO_ENCODING_SLINEAR_LE;
		format_str = "pcm";
		break;
	case UA_FMT_PCM8:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		sc->sc_altflags |= HAS_8U;
		format_str = "pcm8";
		break;
	case UA_FMT_ALAW:
		enc = AUDIO_ENCODING_ALAW;
		sc->sc_altflags |= HAS_ALAW;
		format_str = "alaw";
		break;
	case UA_FMT_MULAW:
		enc = AUDIO_ENCODING_ULAW;
		sc->sc_altflags |= HAS_MULAW;
		format_str = "mulaw";
		break;
	case UA_FMT_IEEE_FLOAT:
	default:
		printf("%s: ignored setting with format %d\n",
		       sc->sc_dev.dv_xname, format);
		return (USBD_NORMAL_COMPLETION);
	}
#ifdef UAUDIO_DEBUG
	printf("%s: %s: %d-ch %d-bit %d-byte %s,", sc->sc_dev.dv_xname,
	       dir == UE_DIR_IN ? "recording" : "playback",
	       chan, prec, bps, format_str);
	if (asf1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
		printf(" %d-%dHz\n", UA_SAMP_LO(asf1d), UA_SAMP_HI(asf1d));
	} else {
		int r;
		printf(" %d", UA_GETSAMP(asf1d, 0));
		for (r = 1; r < asf1d->bSamFreqType; r++)
			printf(",%d", UA_GETSAMP(asf1d, r));
		printf("Hz\n");
	}
#endif
	ai.alt = id->bAlternateSetting;
	ai.encoding = enc;
	ai.attributes = sed->bmAttributes;
	ai.idesc = id;
	ai.edesc = ed;
	ai.edesc1 = sync_ed;
	ai.asf1desc = asf1d;
	ai.sc_busy = 0;
	if (sc->sc_nalts < UAUDIO_MAX_ALTS)
		uaudio_add_alt(sc, &ai);
#ifdef UAUDIO_DEBUG
	if (ai.attributes & UA_SED_FREQ_CONTROL)
		DPRINTFN(1, ("uaudio_process_as:  FREQ_CONTROL\n"));
	if (ai.attributes & UA_SED_PITCH_CONTROL)
		DPRINTFN(1, ("uaudio_process_as:  PITCH_CONTROL\n"));
#endif
	sc->sc_mode |= (dir == UE_DIR_OUT) ? AUMODE_PLAY : AUMODE_RECORD;

	return (USBD_NORMAL_COMPLETION);
}
#undef offs

usbd_status
uaudio_identify_as(struct uaudio_softc *sc,
		   const usb_config_descriptor_t *cdesc)
{
	const usb_interface_descriptor_t *id;
	const char *buf;
	int size, offs;

	size = UGETW(cdesc->wTotalLength);
	buf = (const char *)cdesc;

	/* Locate the AudioStreaming interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOSTREAM,
	    sc->sc_quirks);
	if (id == NULL)
		return (USBD_INVAL);

	/* Loop through all the alternate settings. */
	while (offs <= size) {
		DPRINTFN(2, ("uaudio_identify: interface=%d offset=%d\n",
		    id->bInterfaceNumber, offs));
		switch (id->bNumEndpoints) {
		case 0:
			DPRINTFN(2, ("uaudio_identify: AS null alt=%d\n",
				     id->bAlternateSetting));
			sc->sc_nullalt = id->bAlternateSetting;
			break;
		case 1:
		case 2:
			uaudio_process_as(sc, buf, &offs, size, id);
			break;
		default:
			printf("%s: ignored audio interface with %d "
			       "endpoints\n",
			       sc->sc_dev.dv_xname, id->bNumEndpoints);
			break;
		}
		id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOSTREAM,
		    sc->sc_quirks);
		if (id == NULL)
			break;
	}
	if (offs > size)
		return (USBD_INVAL);
	DPRINTF(("uaudio_identify_as: %d alts available\n", sc->sc_nalts));

	if (sc->sc_mode == 0) {
		printf("%s: no usable endpoint found\n",
		       sc->sc_dev.dv_xname);
		return (USBD_INVAL);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uaudio_identify_ac(struct uaudio_softc *sc, const usb_config_descriptor_t *cdesc)
{
	struct io_terminal* iot;
	const usb_interface_descriptor_t *id;
	const struct usb_audio_control_descriptor *acdp;
	const usb_descriptor_t *dp;
	const struct usb_audio_output_terminal *pot;
	struct terminal_list *tml;
	const char *buf, *ibuf, *ibufend;
	int size, offs, aclen, ndps, i, j;

	size = UGETW(cdesc->wTotalLength);
	buf = (char *)cdesc;

	/* Locate the AudioControl interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOCONTROL,
	    sc->sc_quirks);
	if (id == NULL)
		return (USBD_INVAL);
	if (offs + sizeof *acdp > size)
		return (USBD_INVAL);
	sc->sc_ac_iface = id->bInterfaceNumber;
	DPRINTFN(2,("uaudio_identify_ac: AC interface is %d\n", sc->sc_ac_iface));

	/* A class-specific AC interface header should follow. */
	ibuf = buf + offs;
	acdp = (const struct usb_audio_control_descriptor *)ibuf;
	if (acdp->bDescriptorType != UDESC_CS_INTERFACE ||
	    acdp->bDescriptorSubtype != UDESCSUB_AC_HEADER)
		return (USBD_INVAL);
	aclen = UGETW(acdp->wTotalLength);
	if (offs + aclen > size)
		return (USBD_INVAL);

	if (!(sc->sc_quirks & UAUDIO_FLAG_BAD_ADC) &&
	     UGETW(acdp->bcdADC) != UAUDIO_VERSION)
		return (USBD_INVAL);

	sc->sc_audio_rev = UGETW(acdp->bcdADC);
	DPRINTFN(2,("uaudio_identify_ac: found AC header, vers=%03x, len=%d\n",
		 sc->sc_audio_rev, aclen));

	sc->sc_nullalt = -1;

	/* Scan through all the AC specific descriptors */
	ibufend = ibuf + aclen;
	dp = (const usb_descriptor_t *)ibuf;
	ndps = 0;
	iot = malloc(sizeof(struct io_terminal) * 256, M_TEMP, M_NOWAIT | M_ZERO);
	if (iot == NULL) {
		printf("%s: no memory\n", __func__);
		return USBD_NOMEM;
	}
	for (;;) {
		ibuf += dp->bLength;
		if (ibuf >= ibufend)
			break;
		dp = (const usb_descriptor_t *)ibuf;
		if (ibuf + dp->bLength > ibufend) {
			free(iot, M_TEMP, 0);
			return (USBD_INVAL);
		}
		if (dp->bDescriptorType != UDESC_CS_INTERFACE) {
			printf("uaudio_identify_ac: skip desc type=0x%02x\n",
			       dp->bDescriptorType);
			continue;
		}
		i = ((const struct usb_audio_input_terminal *)dp)->bTerminalId;
		iot[i].d.desc = dp;
		if (i > ndps)
			ndps = i;
	}
	ndps++;

	/* construct io_terminal */
	for (i = 0; i < ndps; i++) {
		dp = iot[i].d.desc;
		if (dp == NULL)
			continue;
		if (dp->bDescriptorSubtype != UDESCSUB_AC_OUTPUT)
			continue;
		pot = iot[i].d.ot;
		tml = uaudio_io_terminaltype(UGETW(pot->wTerminalType), iot, i);
		if (tml != NULL)
			free(tml, M_TEMP, 0);
	}

#ifdef UAUDIO_DEBUG
	for (i = 0; i < 256; i++) {
		if (iot[i].d.desc == NULL)
			continue;
		printf("id %d:\t", i);
		switch (iot[i].d.desc->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			printf("AC_INPUT type=%s\n", uaudio_get_terminal_name
			       (UGETW(iot[i].d.it->wTerminalType)));
			break;
		case UDESCSUB_AC_OUTPUT:
			printf("AC_OUTPUT type=%s ", uaudio_get_terminal_name
			       (UGETW(iot[i].d.ot->wTerminalType)));
			printf("src=%d\n", iot[i].d.ot->bSourceId);
			break;
		case UDESCSUB_AC_MIXER:
			printf("AC_MIXER src=");
			for (j = 0; j < iot[i].d.mu->bNrInPins; j++)
				printf("%d ", iot[i].d.mu->baSourceId[j]);
			printf("\n");
			break;
		case UDESCSUB_AC_SELECTOR:
			printf("AC_SELECTOR src=");
			for (j = 0; j < iot[i].d.su->bNrInPins; j++)
				printf("%d ", iot[i].d.su->baSourceId[j]);
			printf("\n");
			break;
		case UDESCSUB_AC_FEATURE:
			printf("AC_FEATURE src=%d\n", iot[i].d.fu->bSourceId);
			break;
		case UDESCSUB_AC_PROCESSING:
			printf("AC_PROCESSING src=");
			for (j = 0; j < iot[i].d.pu->bNrInPins; j++)
				printf("%d ", iot[i].d.pu->baSourceId[j]);
			printf("\n");
			break;
		case UDESCSUB_AC_EXTENSION:
			printf("AC_EXTENSION src=");
			for (j = 0; j < iot[i].d.eu->bNrInPins; j++)
				printf("%d ", iot[i].d.eu->baSourceId[j]);
			printf("\n");
			break;
		default:
			printf("unknown audio control (subtype=%d)\n",
			       iot[i].d.desc->bDescriptorSubtype);
		}
		for (j = 0; j < iot[i].inputs_size; j++) {
			int k;
			printf("\tinput%d: ", j);
			tml = iot[i].inputs[j];
			if (tml == NULL) {
				printf("NULL\n");
				continue;
			}
			for (k = 0; k < tml->size; k++)
				printf("%s ", uaudio_get_terminal_name
				       (tml->terminals[k]));
			printf("\n");
		}
		printf("\toutput: ");
		tml = iot[i].output;
		for (j = 0; j < tml->size; j++)
			printf("%s ", uaudio_get_terminal_name(tml->terminals[j]));
		printf("\n");
	}
#endif

	for (i = 0; i < ndps; i++) {
		dp = iot[i].d.desc;
		if (dp == NULL)
			continue;
		DPRINTF(("uaudio_identify_ac: id=%d subtype=%d\n",
			 i, dp->bDescriptorSubtype));
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_HEADER:
			printf("uaudio_identify_ac: unexpected AC header\n");
			break;
		case UDESCSUB_AC_INPUT:
			uaudio_add_input(sc, iot, i);
			break;
		case UDESCSUB_AC_OUTPUT:
			uaudio_add_output(sc, iot, i);
			break;
		case UDESCSUB_AC_MIXER:
			uaudio_add_mixer(sc, iot, i);
			break;
		case UDESCSUB_AC_SELECTOR:
			uaudio_add_selector(sc, iot, i);
			break;
		case UDESCSUB_AC_FEATURE:
			uaudio_add_feature(sc, iot, i);
			break;
		case UDESCSUB_AC_PROCESSING:
			uaudio_add_processing(sc, iot, i);
			break;
		case UDESCSUB_AC_EXTENSION:
			uaudio_add_extension(sc, iot, i);
			break;
		default:
			printf("uaudio_identify_ac: bad AC desc subtype=0x%02x\n",
			       dp->bDescriptorSubtype);
			break;
		}
	}

	/* delete io_terminal */
	for (i = 0; i < 256; i++) {
		if (iot[i].d.desc == NULL)
			continue;
		if (iot[i].inputs != NULL) {
			for (j = 0; j < iot[i].inputs_size; j++) {
				if (iot[i].inputs[j] != NULL)
					free(iot[i].inputs[j], M_TEMP, 0);
			}
			free(iot[i].inputs, M_TEMP, 0);
		}
		if (iot[i].output != NULL)
			free(iot[i].output, M_TEMP, 0);
		iot[i].d.desc = NULL;
	}
	free(iot, M_TEMP, 0);

	return (USBD_NORMAL_COMPLETION);
}

int
uaudio_query_devinfo(void *addr, mixer_devinfo_t *mi)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int n, nctls, i;

	DPRINTFN(2,("uaudio_query_devinfo: index=%d\n", mi->index));
	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	n = mi->index;
	nctls = sc->sc_nctls;

	switch (n) {
	case UAC_OUTPUT:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_OUTPUT;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCoutputs, sizeof(mi->label.name));
		return (0);
	case UAC_INPUT:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_INPUT;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCinputs, sizeof(mi->label.name));
		return (0);
	case UAC_EQUAL:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_EQUAL;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCequalization,
		    sizeof(mi->label.name));
		return (0);
	case UAC_RECORD:
		mi->type = AUDIO_MIXER_CLASS;
		mi->mixer_class = UAC_RECORD;
		mi->next = mi->prev = AUDIO_MIXER_LAST;
		strlcpy(mi->label.name, AudioCrecord, sizeof(mi->label.name));
		return 0;
	default:
		break;
	}

	n -= UAC_NCLASSES;
	if (n < 0 || n >= nctls)
		return (ENXIO);

	mc = &sc->sc_ctls[n];
	strlcpy(mi->label.name, mc->ctlname, sizeof(mi->label.name));
	mi->mixer_class = mc->class;
	mi->next = mi->prev = AUDIO_MIXER_LAST;	/* XXX */
	switch (mc->type) {
	case MIX_ON_OFF:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = 2;
		strlcpy(mi->un.e.member[0].label.name, AudioNoff,
		    sizeof(mi->un.e.member[0].label.name));
		mi->un.e.member[0].ord = 0;
		strlcpy(mi->un.e.member[1].label.name, AudioNon,
		    sizeof(mi->un.e.member[1].label.name));
		mi->un.e.member[1].ord = 1;
		break;
	case MIX_SELECTOR:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = mc->maxval - mc->minval + 1;
		for (i = 0; i <= mc->maxval - mc->minval; i++) {
			snprintf(mi->un.e.member[i].label.name,
				 sizeof(mi->un.e.member[i].label.name),
				 "%d", i + mc->minval);
			mi->un.e.member[i].ord = i + mc->minval;
		}
		break;
	default:
		mi->type = AUDIO_MIXER_VALUE;
		strlcpy(mi->un.v.units.name, mc->ctlunit,
		    sizeof(mi->un.v.units.name));
		mi->un.v.num_channels = mc->nchan;
		mi->un.v.delta = mc->delta;
		break;
	}
	return (0);
}

int
uaudio_open(void *addr, int flags)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_open: sc=%p\n", sc));
	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	if ((flags & FWRITE) && !(sc->sc_mode & AUMODE_PLAY))
		return (ENXIO);
	if ((flags & FREAD) && !(sc->sc_mode & AUMODE_RECORD))
		return (ENXIO);

	return (0);
}

/*
 * Close function is called at splaudio().
 */
void
uaudio_close(void *addr)
{
	struct uaudio_softc *sc = addr;

	if (sc->sc_playchan.altidx != -1)
		uaudio_chan_close(sc, &sc->sc_playchan);
	if (sc->sc_recchan.altidx != -1)
		uaudio_chan_close(sc, &sc->sc_recchan);
}

int
uaudio_drain(void *addr)
{
	struct uaudio_softc *sc = addr;
	struct chan *pchan = &sc->sc_playchan;
	struct chan *rchan = &sc->sc_recchan;
	int ms = 0;

	/* Wait for outstanding requests to complete. */
	if (pchan->altidx != -1 && sc->sc_alts[pchan->altidx].sc_busy)
		ms = max(ms, pchan->reqms);
	if (rchan->altidx != -1 && sc->sc_alts[rchan->altidx].sc_busy)
		ms = max(ms, rchan->reqms);
	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * ms);

	return (0);
}

int
uaudio_halt_out_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_halt_out_dma: enter\n"));
	if (sc->sc_playchan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_playchan);
		sc->sc_playchan.pipe = NULL;
		if (sc->sc_playchan.sync_pipe != NULL)
			sc->sc_playchan.sync_pipe = NULL;
		uaudio_chan_free_buffers(sc, &sc->sc_playchan);
		sc->sc_playchan.intr = NULL;
	}
	return (0);
}

int
uaudio_halt_in_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_halt_in_dma: enter\n"));
	if (sc->sc_recchan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_recchan);
		sc->sc_recchan.pipe = NULL;
		if (sc->sc_recchan.sync_pipe != NULL)
			sc->sc_recchan.sync_pipe = NULL;
		uaudio_chan_free_buffers(sc, &sc->sc_recchan);
		sc->sc_recchan.intr = NULL;
	}
	return (0);
}

int
uaudio_getdev(void *addr, struct audio_device *retp)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_mixer_getdev:\n"));
	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	*retp = uaudio_device;
	return (0);
}

/*
 * Make sure the block size is large enough to hold at least 1 transfer.
 * Ideally, the block size should be a multiple of the transfer size.
 * Currently, the transfer size for play and record can differ, and there's
 * no way to round playback and record blocksizes separately.
 */
int
uaudio_round_blocksize(void *addr, int blk)
{
	struct uaudio_softc *sc = addr;
	int bpf, pbpf, rbpf;

	DPRINTF(("uaudio_round_blocksize: p.mbpf=%d r.mbpf=%d\n",
		 sc->sc_playchan.max_bytes_per_frame,
		 sc->sc_recchan.max_bytes_per_frame));

	pbpf = rbpf = 0;
	if (sc->sc_mode & AUMODE_PLAY) {
		pbpf = (sc->sc_playchan.max_bytes_per_frame) *
		    sc->sc_playchan.nframes;
	}
	if (sc->sc_mode & AUMODE_RECORD) {
		rbpf = (sc->sc_recchan.max_bytes_per_frame) *
		    sc->sc_recchan.nframes;
	}
	bpf = max(pbpf, rbpf);

	if (blk < bpf)
		blk = bpf;

#ifdef DIAGNOSTIC
	if (blk <= 0) {
		printf("uaudio_round_blocksize: blk=%d\n", blk);
		blk = 512;
	}
#endif

	DPRINTFN(1,("uaudio_round_blocksize: blk=%d\n", blk));
	return (blk);
}

int
uaudio_get_props(void *addr)
{
	struct uaudio_softc *sc = addr;
	int props = 0;

	if (!(sc->sc_quirks & UAUDIO_FLAG_DEPENDENT))
		props |= AUDIO_PROP_INDEPENDENT;

	if ((sc->sc_mode & (AUMODE_PLAY | AUMODE_RECORD)) ==
	    (AUMODE_PLAY | AUMODE_RECORD))
		props |= AUDIO_PROP_FULLDUPLEX;

	return props;
}

void
uaudio_get_default_params(void *addr, int mode, struct audio_params *p)
{
	struct uaudio_softc *sc = addr;
	int flags, alt;

	/* try aucat(1) defaults: 44100 Hz stereo s16le */
	p->sample_rate = 44100;
	p->encoding = AUDIO_ENCODING_SLINEAR_LE;
	p->precision = 16;
	p->bps = 2;
	p->msb = 1;
	p->channels = 2;
	p->sw_code = NULL;
	p->factor = 1;

	/* If the device doesn't support the current mode, there's no
	 * need to find better parameters.
	 */
	if (!(sc->sc_mode & mode))
		return;

	flags = sc->sc_altflags;
	if (flags & HAS_16)
		;
	else if (flags & HAS_24)
		p->precision = 24;
	else {
		p->precision = 8;
		if (flags & HAS_8)
			;
		else if (flags & HAS_8U)
			p->encoding = AUDIO_ENCODING_ULINEAR_LE;
		else if (flags & HAS_MULAW)
			p->encoding = AUDIO_ENCODING_ULAW;
		else if (flags & HAS_ALAW)
			p->encoding = AUDIO_ENCODING_ALAW;
	}

	alt = uaudio_match_alt(sc, p, mode);
	if (alt != -1)
		p->bps = sc->sc_alts[alt].asf1desc->bSubFrameSize;
}

int
uaudio_get(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len)
{
	usb_device_request_t req;
	u_int8_t data[4];
	usbd_status err;
	int val;

	if (wValue == -1)
		return (0);

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	DPRINTFN(2,("uaudio_get: type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d\n",
		    type, which, wValue, wIndex, len));
	err = usbd_do_request(sc->sc_udev, &req, data);
	if (err) {
		DPRINTF(("uaudio_get: err=%s\n", usbd_errstr(err)));
		return (-1);
	}
	switch (len) {
	case 1:
		val = data[0];
		break;
	case 2:
		val = data[0] | (data[1] << 8);
		break;
	default:
		DPRINTF(("uaudio_get: bad length=%d\n", len));
		return (-1);
	}
	DPRINTFN(2,("uaudio_get: val=%d\n", val));
	return (val);
}

void
uaudio_set(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len, int val)
{
	usb_device_request_t req;
	u_int8_t data[4];
	usbd_status err;

	if (wValue == -1)
		return;

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	switch (len) {
	case 1:
		data[0] = val;
		break;
	case 2:
		data[0] = val;
		data[1] = val >> 8;
		break;
	default:
		return;
	}
	DPRINTFN(2,("uaudio_set: type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d, val=%d\n",
		    type, which, wValue, wIndex, len, val & 0xffff));
	err = usbd_do_request(sc->sc_udev, &req, data);
#ifdef UAUDIO_DEBUG
	if (err)
		DPRINTF(("uaudio_set: err=%d\n", err));
#endif
}

int
uaudio_signext(int type, int val)
{
	if (!MIX_UNSIGNED(type)) {
		if (MIX_SIZE(type) == 2)
			val = (int16_t)val;
		else
			val = (int8_t)val;
	}
	return (val);
}

int
uaudio_unsignext(int type, int val)
{
	if (!MIX_UNSIGNED(type)) {
		if (MIX_SIZE(type) == 2)
			val = (u_int16_t)val;
		else
			val = (u_int8_t)val;
	}
	return (val);
}

int
uaudio_value2bsd(struct mixerctl *mc, int val)
{
	int range;
	DPRINTFN(5, ("uaudio_value2bsd: type=%03x val=%d min=%d max=%d ",
		     mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF) {
		val = (val != 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (val < mc->minval || val > mc->maxval)
			val = mc->minval;
	} else {
		range = mc->maxval - mc->minval;
		if (range == 0) 
			val = 0;
		else
			val = 255 * (uaudio_signext(mc->type, val) - 
			    mc->minval) / range;
	}
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}

int
uaudio_bsd2value(struct mixerctl *mc, int val)
{
	DPRINTFN(5,("uaudio_bsd2value: type=%03x val=%d min=%d max=%d ",
		    mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF) {
		val = (val != 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (val < mc->minval || val > mc->maxval)
			val = mc->minval;
	} else
		val = uaudio_unsignext(mc->type, 
		    val * (mc->maxval - mc->minval) / 255 + mc->minval);
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}

int
uaudio_ctl_get(struct uaudio_softc *sc, int which, struct mixerctl *mc,
	       int chan)
{
	int val;

	DPRINTFN(5,("uaudio_ctl_get: which=%d chan=%d\n", which, chan));
	val = uaudio_get(sc, which, UT_READ_CLASS_INTERFACE, mc->wValue[chan],
			 mc->wIndex, MIX_SIZE(mc->type));
	return (uaudio_value2bsd(mc, val));
}

void
uaudio_ctl_set(struct uaudio_softc *sc, int which, struct mixerctl *mc,
	       int chan, int val)
{
	val = uaudio_bsd2value(mc, val);
	uaudio_set(sc, which, UT_WRITE_CLASS_INTERFACE, mc->wValue[chan],
		   mc->wIndex, MIX_SIZE(mc->type), val);
}

int
uaudio_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN], val;

	DPRINTFN(2,("uaudio_mixer_get_port: index=%d\n", cp->dev));

	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	n = cp->dev - UAC_NCLASSES;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		cp->un.ord = uaudio_ctl_get(sc, GET_CUR, mc, 0);
	} else if (mc->type == MIX_SELECTOR) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		cp->un.ord = uaudio_ctl_get(sc, GET_CUR, mc, 0);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);
		if (cp->un.value.num_channels != 1 &&
		    cp->un.value.num_channels != mc->nchan)
			return (EINVAL);
		for (i = 0; i < mc->nchan; i++)
			vals[i] = uaudio_ctl_get(sc, GET_CUR, mc, i);
		if (cp->un.value.num_channels == 1 && mc->nchan != 1) {
			for (val = 0, i = 0; i < mc->nchan; i++)
				val += vals[i];
			vals[0] = val / mc->nchan;
		}
		for (i = 0; i < cp->un.value.num_channels; i++)
			cp->un.value.level[i] = vals[i];
	}

	return (0);
}

int
uaudio_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN];

	DPRINTFN(2,("uaudio_mixer_set_port: index = %d\n", cp->dev));
	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	n = cp->dev - UAC_NCLASSES;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		uaudio_ctl_set(sc, SET_CUR, mc, 0, cp->un.ord);
	} else if (mc->type == MIX_SELECTOR) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		uaudio_ctl_set(sc, SET_CUR, mc, 0, cp->un.ord);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);
		if (cp->un.value.num_channels == 1)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[0];
		else if (cp->un.value.num_channels == mc->nchan)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[i];
		else
			return (EINVAL);
		for (i = 0; i < mc->nchan; i++)
			uaudio_ctl_set(sc, SET_CUR, mc, i, vals[i]);
	}
	return (0);
}

int
uaudio_trigger_input(void *addr, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     struct audio_params *param)
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_recchan;
	usbd_status err;
	int i, s;

	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_input: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, start, end, blksize);
	DPRINTFN(3,("uaudio_trigger_input: sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction));

	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err)
		return (EIO);

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		return (EIO);
	}

	ch->intr = intr;
	ch->arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		uaudio_chan_rtransfer(ch);
	splx(s);

	return (0);
}

int
uaudio_trigger_output(void *addr, void *start, void *end, int blksize,
		      void (*intr)(void *), void *arg,
		      struct audio_params *param)
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_playchan;
	usbd_status err;
	int i, s;

	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_output: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, start, end, blksize);
	DPRINTFN(3,("uaudio_trigger_output: sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction));

	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err)
		return (EIO);

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		return (EIO);
	}

	ch->intr = intr;
	ch->arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		uaudio_chan_ptransfer(ch);
	if (ch->sync_pipe) {
		for (i = 0; i < UAUDIO_NSYNCBUFS; i++)
			uaudio_chan_psync_transfer(ch);
	}
	splx(s);

	return (0);
}

/* Set up a pipe for a channel. */
usbd_status
uaudio_chan_open(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[ch->altidx];
	int endpt = as->edesc->bEndpointAddress;
	usbd_status err;

	DPRINTF(("uaudio_chan_open: endpt=0x%02x, speed=%d, alt=%d\n",
		 endpt, ch->sample_rate, as->alt));

	/* Set alternate interface corresponding to the mode. */
	err = usbd_set_interface(as->ifaceh, as->alt);
	if (err) {
		DPRINTF(("%s: usbd_set_interface failed\n", __func__));
		return (err);
	}

	/*
	 * If just one sampling rate is supported,
	 * no need to call uaudio_set_speed().
	 * Roland SD-90 freezes by a SAMPLING_FREQ_CONTROL request.
	 */
	if (as->asf1desc->bSamFreqType != 1) {
		err = uaudio_set_speed(sc, endpt, ch->sample_rate);
		if (err)
			DPRINTF(("uaudio_chan_open: set_speed failed err=%s\n",
				 usbd_errstr(err)));
	}

	if (sc->sc_quirks & UAUDIO_FLAG_EMU0202)
		uaudio_set_speed_emu0202(ch);

	ch->pipe = 0;
	ch->sync_pipe = 0;
	DPRINTF(("uaudio_chan_open: create pipe to 0x%02x\n", endpt));
	err = usbd_open_pipe(as->ifaceh, endpt, 0, &ch->pipe);
	if (err) {
		printf("%s: error creating pipe: err=%s endpt=0x%02x\n",
		    __func__, usbd_errstr(err), endpt);
		return err;
	}
	if (as->edesc1 != NULL) {
		endpt = as->edesc1->bEndpointAddress;
		DPRINTF(("uaudio_chan_open: create sync-pipe to 0x%02x\n", endpt));
		err = usbd_open_pipe(as->ifaceh, endpt, 0, &ch->sync_pipe);
		if (err) {
			printf("%s: error creating sync-pipe: err=%s endpt=0x%02x\n",
			    __func__, usbd_errstr(err), endpt);
		}
	}
	return err;
}

void
uaudio_chan_close(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[ch->altidx];

	as->sc_busy = 0;
	if (sc->sc_nullalt >= 0) {
		DPRINTF(("uaudio_chan_close: set null alt=%d\n",
			 sc->sc_nullalt));
		usbd_set_interface(as->ifaceh, sc->sc_nullalt);
	}
	if (ch->pipe) {
		usbd_abort_pipe(ch->pipe);
		usbd_close_pipe(ch->pipe);
	}
	if (ch->sync_pipe) {
		usbd_abort_pipe(ch->sync_pipe);
		usbd_close_pipe(ch->sync_pipe);
	}
}

usbd_status
uaudio_chan_alloc_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[ch->altidx];
	struct usbd_xfer *xfer;
	void *buf;
	int i, size;

	DPRINTF(("%s: max_bytes_per_frame=%d nframes=%d\n", __func__,
	    ch->max_bytes_per_frame, ch->nframes));

	size = ch->max_bytes_per_frame * ch->nframes;
	for (i = 0; i < UAUDIO_NCHANBUFS; i++) {
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			goto bad;
		ch->chanbufs[i].xfer = xfer;
		buf = usbd_alloc_buffer(xfer, size);
		if (buf == 0) {
			i++;
			goto bad;
		}
		ch->chanbufs[i].buffer = buf;
		ch->chanbufs[i].chan = ch;
	}
	if (as->edesc1 != NULL) {
		size = (ch->hi_speed ? 4 : 3) * ch->nsync_frames;
		for (i = 0; i < UAUDIO_NSYNCBUFS; i++) {
			xfer = usbd_alloc_xfer(sc->sc_udev);
			if (xfer == 0)
				goto bad_sync;
			ch->syncbufs[i].xfer = xfer;
			buf = usbd_alloc_buffer(xfer, size);
			if (buf == 0) {
				i++;
				goto bad_sync;
			}
			ch->syncbufs[i].buffer = buf;
			ch->syncbufs[i].chan = ch;
		}
	}

	return (USBD_NORMAL_COMPLETION);

bad:
	while (--i >= 0)
		/* implicit buffer free */
		usbd_free_xfer(ch->chanbufs[i].xfer);
	return (USBD_NOMEM);

bad_sync:
	while (--i >= 0)
		/* implicit buffer free */
		usbd_free_xfer(ch->syncbufs[i].xfer);
	return (USBD_NOMEM);

}

void
uaudio_chan_free_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[ch->altidx];
	int i;

	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		usbd_free_xfer(ch->chanbufs[i].xfer);
	if (as->edesc1 != NULL) {
		for (i = 0; i < UAUDIO_NSYNCBUFS; i++)
			usbd_free_xfer(ch->syncbufs[i].xfer);
	}
}

/* Called at splusb() */
void
uaudio_chan_ptransfer(struct chan *ch)
{
	struct chanbuf *cb;
	u_char *pos;
	int i, n, size, residue, total;

	if (usbd_is_dying(ch->sc->sc_udev))
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < ch->nframes; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= ch->frac_denom) {
			if ((ch->sc->sc_altflags & UA_NOFRAC) == 0)
				size += ch->sample_size;
			residue -= ch->frac_denom;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

	/*
	 * Transfer data from upper layer buffer to channel buffer.  Be sure
	 * to let the upper layer know each time a block is moved, so it can
	 * add more.
	 */
	pos = cb->buffer;
	while (total > 0) {
		n = min(total, ch->end - ch->cur);
		n = min(n, ch->blksize - ch->transferred);
		memcpy(pos, ch->cur, n);
		total -= n;
		pos += n;
		ch->cur += n;
		if (ch->cur >= ch->end)
			ch->cur = ch->start;

		ch->transferred += n;
		/* Call back to upper layer */
		if (ch->transferred >= ch->blksize) {
			DPRINTFN(5,("uaudio_chan_ptransfer: call %p(%p)\n",
				    ch->intr, ch->arg));
			mtx_enter(&audio_lock);
			ch->intr(ch->arg);
			mtx_leave(&audio_lock);
			ch->transferred -= ch->blksize;
		}
	}

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_ptransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < ch->nframes; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_ptransfer: transfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes,
			     ch->nframes, USBD_NO_COPY,
			     uaudio_chan_pintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_pintr(struct usbd_xfer *xfer, void *priv,
		  usbd_status status)
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int32_t count;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_pintr: count=%d, transferred=%d\n",
		    count, ch->transferred));
#ifdef UAUDIO_DEBUG
	if (count != cb->size) {
		printf("uaudio_chan_pintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	/* start next transfer */
	uaudio_chan_ptransfer(ch);
}

/* Called at splusb() */
void
uaudio_chan_psync_transfer(struct chan *ch)
{
	struct syncbuf *sb;
	int i, size, total = 0;

	if (usbd_is_dying(ch->sc->sc_udev))
		return;

	/* Pick the next sync buffer. */
	sb = &ch->syncbufs[ch->cursyncbuf];
	if (++ch->cursyncbuf >= UAUDIO_NSYNCBUFS)
		ch->cursyncbuf = 0;

	size = ch->hi_speed ? 4 : 3;
	for (i = 0; i < ch->nsync_frames; i++) {
		sb->sizes[i] = size;
		sb->offsets[i] = total;
		total += size;
	}
	sb->size = total;

	DPRINTFN(5,("%s: transfer xfer=%p\n", __func__, sb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(sb->xfer, ch->sync_pipe, sb, sb->sizes,
	    ch->nsync_frames, USBD_NO_COPY, uaudio_chan_psync_intr);

	(void)usbd_transfer(sb->xfer);
}

void
uaudio_chan_psync_intr(struct usbd_xfer *xfer, void *priv,
    usbd_status status)
{
	struct syncbuf *sb = priv;
	struct chan *ch = sb->chan;
	u_int32_t count, tmp;
	u_int32_t freq, freq_w, freq_f;
	int i, pos, size;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("%s: count=%d\n", __func__, count));

	size = ch->hi_speed ? 4 : 3;
	for (i = 0; count > 0 && i < ch->nsync_frames; i++) {
		if (sb->sizes[i] != size)
			continue;
		count -= size;
		pos = sb->offsets[i];
		if (ch->hi_speed) {
			/* 16.16 (12.13) -> 16.16 (12.16) */
			freq = sb->buffer[pos+3] << 24 |
			    sb->buffer[pos+2] << 16 |
			    sb->buffer[pos+1] << 8 |
			    sb->buffer[pos];
		} else {
			/* 10.14 (10.10) -> 16.16 (10.16) */
			freq = sb->buffer[pos+2] << 18 |
			    sb->buffer[pos+1] << 10 |
			    sb->buffer[pos] << 2;
		}
		freq_w = (freq >> 16) & (ch->hi_speed ? 0x0fff : 0x03ff);
		freq_f = freq & 0xffff;
		DPRINTFN(5,("%s: freq = %d %d/%d\n", __func__, freq_w, freq_f,
		    ch->frac_denom));
		tmp = freq_w * ch->sample_size;
		if (tmp + (freq_f ? ch->sample_size : 0) >
		    ch->max_bytes_per_frame) {
			DPRINTF(("%s: packet size request too large: %d/%d/%d\n",
			    __func__, tmp, ch->max_bytes_per_frame, ch->maxpktsize));
		} else {
			ch->bytes_per_frame = tmp;
			ch->fraction = freq_f;
		}
	}

	/* start next transfer */
	uaudio_chan_psync_transfer(ch);
}

/* Called at splusb() */
void
uaudio_chan_rtransfer(struct chan *ch)
{
	struct chanbuf *cb;
	int i, size, residue, total;

	if (usbd_is_dying(ch->sc->sc_udev))
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < ch->nframes; i++) {
		size = ch->bytes_per_frame;
		cb->sizes[i] = size;
		cb->offsets[i] = total;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_rtransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < ch->nframes; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_rtransfer: transfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes,
			     ch->nframes, USBD_NO_COPY,
			     uaudio_chan_rintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_rintr(struct usbd_xfer *xfer, void *priv,
		  usbd_status status)
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int16_t pos;
	u_int32_t count;
	int i, n, frsize;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_rintr: count=%d, transferred=%d\n",
		    count, ch->transferred));

	/* count < cb->size is normal for asynchronous source */
#ifdef DIAGNOSTIC
	if (count > cb->size) {
		printf("uaudio_chan_rintr: count(%d) > size(%d)\n",
		       count, cb->size);
	}
#endif

	/*
	 * Transfer data from channel buffer to upper layer buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	for (i = 0; i < ch->nframes; i++) {
		frsize = cb->sizes[i];
		pos = cb->offsets[i];
		while (frsize > 0) {
			n = min(frsize, ch->end - ch->cur);
			n = min(n, ch->blksize - ch->transferred);
			memcpy(ch->cur, cb->buffer + pos, n);
			frsize -= n;
			pos += n;
			ch->cur += n;
			if (ch->cur >= ch->end)
				ch->cur = ch->start;

			ch->transferred += n;
			/* Call back to upper layer */
			if (ch->transferred >= ch->blksize) {
				DPRINTFN(5,("uaudio_chan_rintr: call %p(%p)\n",
					    ch->intr, ch->arg));
				mtx_enter(&audio_lock);
				ch->intr(ch->arg);
				mtx_leave(&audio_lock);
				ch->transferred -= ch->blksize;
			}
			if (count < n)
				printf("%s: count < n\n", __func__);
			else
				count -= n;
		}
	}
	if (count != 0) {
		printf("%s: transfer count - frame total = %d\n",
		    __func__, count);
	}

	/* start next transfer */
	uaudio_chan_rtransfer(ch);
}

void
uaudio_chan_init(struct chan *ch, int mode, int altidx,
    const struct audio_params *param)
{
	struct as_info *ai = &ch->sc->sc_alts[altidx];
	int samples_per_frame, ival, use_maxpkt = 0;

	if (ai->attributes & UA_SED_MAXPACKETSONLY) {
		DPRINTF(("%s: alt %d needs maxpktsize packets\n",
		    __func__, altidx));
		use_maxpkt = 1;
	}
	if (mode == AUMODE_RECORD) {
		DPRINTF(("%s: using maxpktsize packets for record channel\n",
		    __func__));
		use_maxpkt = 1;
	}

	ch->altidx = altidx;
	ch->maxpktsize = UGETW(ai->edesc->wMaxPacketSize);
	ch->sample_rate = param->sample_rate;
	ch->sample_size = param->channels * param->bps;
	ch->usb_fps = USB_FRAMES_PER_SECOND;
	ch->hi_speed = ch->sc->sc_udev->speed == USB_SPEED_HIGH;
	if (ch->hi_speed) {
		ch->usb_fps *= 8;
		/*
		 * Polling interval is considered a frame, as opposed to
		 * micro-frame being a frame.
		 */
		ival = ch->sc->sc_alts[altidx].edesc->bInterval;
		if (ival > 0 && ival <= 4)
			ch->usb_fps >>= (ival - 1);
		DPRINTF(("%s: detected USB high-speed with ival %d\n",
		    __func__, ival));
	}

	/*
	 * Use UAUDIO_MIN_FRAMES here, so uaudio_round_blocksize() can
	 * make sure the blocksize duration will be > 1 USB frame.
	 */
	samples_per_frame = ch->sample_rate / ch->usb_fps;
	if (!use_maxpkt) {
		ch->fraction = ch->sample_rate % ch->usb_fps;
		if (samples_per_frame * ch->sample_size > ch->maxpktsize) {
			DPRINTF(("%s: packet size %d too big, max %d\n",
			    __func__, ch->bytes_per_frame, ch->maxpktsize));
			samples_per_frame = ch->maxpktsize / ch->sample_size;
		}
		ch->bytes_per_frame = samples_per_frame * ch->sample_size;
		ch->nframes = UAUDIO_MIN_FRAMES;
	} else {
		ch->fraction = 0;
		ch->bytes_per_frame = ch->maxpktsize;
		ch->nframes = UAUDIO_MIN_FRAMES * samples_per_frame *
		    ch->sample_size / ch->maxpktsize;
	}
	if (ch->nframes > UAUDIO_MAX_FRAMES)
		ch->nframes = UAUDIO_MAX_FRAMES;
	else if (ch->nframes < 1)
		ch->nframes = 1;

	ch->max_bytes_per_frame = ch->bytes_per_frame;
	if (!use_maxpkt)
		ch->max_bytes_per_frame += ch->sample_size;
	if (ch->max_bytes_per_frame > ch->maxpktsize)
		ch->max_bytes_per_frame = ch->maxpktsize;

	ch->residue = 0;
	ch->frac_denom = ch->usb_fps;
	if (ai->edesc1 != NULL) {
		/*
		 * The lower 16-bits of the sync request represent
		 * fractional samples.  Scale up the fraction here once
		 * so all fractions are using the same denominator.
		 */
		ch->frac_denom = 1 << 16;
		ch->fraction = (ch->fraction * ch->frac_denom) / ch->usb_fps;

		/*
		 * Have to set nsync_frames somewhere.  We can request
		 * a lot of sync data; the device will reply when it's
		 * ready, with empty frames meaning to keep using the
		 * current rate.
		 */
		ch->nsync_frames = UAUDIO_MAX_FRAMES;
	}
	DPRINTF(("%s: residual sample fraction: %d/%d\n", __func__,
	    ch->fraction, ch->frac_denom));
}

void
uaudio_chan_set_param(struct chan *ch, u_char *start, u_char *end, int blksize)
{
	ch->start = start;
	ch->end = end;
	ch->cur = start;
	ch->transferred = 0;
	ch->curchanbuf = 0;
	ch->blksize = blksize;

	/*
	 * Recompute nframes based on blksize, but make sure nframes
	 * is not longer in time duration than blksize.
	 */
	ch->nframes = ch->blksize * ch->usb_fps /
	    (ch->bytes_per_frame * ch->usb_fps +
	    ch->sample_size * ch->fraction);
	if (ch->nframes > UAUDIO_MAX_FRAMES)
		ch->nframes = UAUDIO_MAX_FRAMES;
	else if (ch->nframes < 1)
		ch->nframes = 1;

	ch->reqms = ch->bytes_per_frame / ch->sample_size *
	    ch->nframes * 1000 / ch->sample_rate;

	DPRINTF(("%s: alt=%d blk=%d maxpkt=%u bpf=%u rate=%u nframes=%u reqms=%u\n",
	    __func__, ch->altidx, ch->blksize, ch->maxpktsize,
	    ch->bytes_per_frame, ch->sample_rate, ch->nframes, ch->reqms));
}

int
uaudio_match_alt_rate(void *addr, int alt, int rate)
{
	struct uaudio_softc *sc = addr;
	const struct usb_audio_streaming_type1_descriptor *a1d;
	int i, j, r;

	a1d = sc->sc_alts[alt].asf1desc;
	if (a1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
		if ((UA_SAMP_LO(a1d) <= rate) &&
		    (UA_SAMP_HI(a1d) >= rate)) {
			return rate;
		} else {
			if (UA_SAMP_LO(a1d) > rate)
				return UA_SAMP_LO(a1d);
			else
				return UA_SAMP_HI(a1d);
		}
	} else {
		for (i = 0; i < 100; i++) {
			for (j = 0; j < a1d->bSamFreqType; j++) {
				r = UA_GETSAMP(a1d, j);
				if ((r - (500 * i) <= rate) &&
				    (r + (500 * i) >= rate))
					return r;
			}
		}
		/* assumes rates are listed in order from lowest to highest */
		if (rate < UA_GETSAMP(a1d, 0))
			j = 0;
		else
			j = a1d->bSamFreqType - 1;
		return UA_GETSAMP(a1d, j);
	}
	DPRINTF(("%s: could not match rate\n", __func__));
	return rate;
}

int
uaudio_match_alt(void *addr, struct audio_params *p, int mode)
{
	struct uaudio_softc *sc = addr;
	const struct usb_audio_streaming_type1_descriptor *a1d;
	int i, j, dir, rate;
	int alts_eh, alts_ch, ualt;

	DPRINTF(("%s: mode=%s rate=%ld ch=%d pre=%d bps=%d enc=%d\n",
	    __func__, mode == AUMODE_RECORD ? "rec" : "play", p->sample_rate,
	    p->channels, p->precision, p->bps, p->encoding));

	alts_eh = 0;
	for (i = 0; i < sc->sc_nalts; i++) {
		dir = UE_GET_DIR(sc->sc_alts[i].edesc->bEndpointAddress);
		if ((mode == AUMODE_RECORD && dir != UE_DIR_IN) ||
		    (mode == AUMODE_PLAY && dir == UE_DIR_IN))
			continue;
		DPRINTFN(6,("%s: matched %s alt %d for direction\n", __func__,
		    mode == AUMODE_RECORD ? "rec" : "play", i));
		if (sc->sc_alts[i].encoding != p->encoding)
			continue;
		a1d = sc->sc_alts[i].asf1desc;
		if (a1d->bBitResolution != p->precision)
			continue;
		alts_eh |= 1 << i;
		DPRINTFN(6,("%s: matched %s alt %d for enc/pre\n", __func__,
		    mode == AUMODE_RECORD ? "rec" : "play", i));
	}
	if (alts_eh == 0) {
		DPRINTF(("%s: could not match dir/enc/prec\n", __func__));
		return -1;
	}

	alts_ch = 0;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < sc->sc_nalts; j++) {
			if (!(alts_eh & (1 << j)))
				continue;
			a1d = sc->sc_alts[j].asf1desc;
			if (a1d->bNrChannels == p->channels) {
				alts_ch |= 1 << j;
				DPRINTFN(6,("%s: matched alt %d for channels\n",
				    __func__, j));
			}
		}
		if (alts_ch)
			break;
		if (p->channels == 2)
			p->channels = 1;
		else
			p->channels = 2;
	}
	if (!alts_ch) {
		/* just use the first alt that matched the encoding */
		for (i = 0; i < sc->sc_nalts; i++)
			if (alts_eh & (1 << i))
				break;
		alts_ch = 1 << i;
		a1d = sc->sc_alts[i].asf1desc;
		p->channels = a1d->bNrChannels;
	}

	ualt = -1;
	for (i = 0; i < sc->sc_nalts; i++) {
		if (alts_ch & (1 << i)) {
			rate = uaudio_match_alt_rate(sc, i, p->sample_rate);
			if (rate - 50 <= p->sample_rate &&
			    rate + 50 >= p->sample_rate) {
				DPRINTFN(6,("%s: alt %d matched rate %ld with %d\n",
				    __func__, i, p->sample_rate, rate));
				p->sample_rate = rate;
				break;
			}
		}
	}
	if (i < sc->sc_nalts) {
		ualt = i;
	} else {
		for (i = 0; i < sc->sc_nalts; i++) {
			if (alts_ch & (1 << i)) {
				ualt = i;
				p->sample_rate = uaudio_match_alt_rate(sc,
				    i, p->sample_rate);
				break;
			}
		}
	}

	return ualt;
}

int
uaudio_set_params(void *addr, int setmode, int usemode,
    struct audio_params *play, struct audio_params *rec)
{
	struct uaudio_softc *sc = addr;
	int flags = sc->sc_altflags;
	int i;
	int paltidx = -1, raltidx = -1;
	struct audio_params *p;
	int mode;

	if (usbd_is_dying(sc->sc_udev))
		return (EIO);

	if (((usemode & AUMODE_PLAY) && sc->sc_playchan.pipe != NULL) ||
	    ((usemode & AUMODE_RECORD) && sc->sc_recchan.pipe != NULL))
		return (EBUSY);

	if ((usemode & AUMODE_PLAY) && sc->sc_playchan.altidx != -1)
		sc->sc_alts[sc->sc_playchan.altidx].sc_busy = 0;
	if ((usemode & AUMODE_RECORD) && sc->sc_recchan.altidx != -1)
		sc->sc_alts[sc->sc_recchan.altidx].sc_busy = 0;

	/* Some uaudio devices are unidirectional.  Don't try to find a
	   matching mode for the unsupported direction. */
	setmode &= sc->sc_mode;

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = (mode == AUMODE_PLAY) ? play : rec;

		switch (p->precision) {
		case 24:
			if (!(flags & HAS_24)) {
				if (flags & HAS_16)
					p->precision = 16;
				else
					p->precision = 8;
			}
			break;
		case 16:
			if (!(flags & HAS_16)) {
				if (flags & HAS_24)
					p->precision = 24;
				else
					p->precision = 8;
			}
			break;
		case 8:
			if (!(flags & HAS_8) && !(flags & HAS_8U)) {
				if (flags & HAS_16)
					p->precision = 16;
				else
					p->precision = 24;
			}
			break;
		}

		i = uaudio_match_alt(sc, p, mode);
		if (i < 0) {
			DPRINTF(("%s: uaudio_match_alt failed for %s\n",
			    __func__, mode == AUMODE_RECORD ? "rec" : "play"));
			continue;
		}

		p->sw_code = NULL;
		p->factor  = 1;

		p->bps = sc->sc_alts[i].asf1desc->bSubFrameSize;
		p->msb = 1;

		if (mode == AUMODE_PLAY)
			paltidx = i;
		else
			raltidx = i;
	}

	if (setmode & AUMODE_PLAY) {
		if (paltidx == -1) {
			DPRINTF(("%s: did not find alt for playback\n",
			    __func__));
			return (EINVAL);
		}
		/* XXX abort transfer if currently happening? */
		uaudio_chan_init(&sc->sc_playchan, AUMODE_PLAY, paltidx, play);
	}
	if (setmode & AUMODE_RECORD) {
		if (raltidx == -1) {
			DPRINTF(("%s: did not find alt for recording\n",
			    __func__));
			return (EINVAL);
		}
		/* XXX abort transfer if currently happening? */
		uaudio_chan_init(&sc->sc_recchan, AUMODE_RECORD, raltidx, rec);
	}

	if ((usemode & AUMODE_PLAY) && sc->sc_playchan.altidx != -1)
		sc->sc_alts[sc->sc_playchan.altidx].sc_busy = 1;
	if ((usemode & AUMODE_RECORD) && sc->sc_recchan.altidx != -1)
		sc->sc_alts[sc->sc_recchan.altidx].sc_busy = 1;

	DPRINTF(("uaudio_set_params: use altidx=p%d/r%d, altno=p%d/r%d\n",
		 sc->sc_playchan.altidx, sc->sc_recchan.altidx,
		 (sc->sc_playchan.altidx >= 0)
		   ?sc->sc_alts[sc->sc_playchan.altidx].idesc->bAlternateSetting
		   : -1,
		 (sc->sc_recchan.altidx >= 0)
		   ? sc->sc_alts[sc->sc_recchan.altidx].idesc->bAlternateSetting
		   : -1));

	return (0);
}

usbd_status
uaudio_set_speed(struct uaudio_softc *sc, int endpt, u_int speed)
{
	usb_device_request_t req;
	u_int8_t data[3];

	DPRINTFN(5,("uaudio_set_speed: endpt=%d speed=%u\n", endpt, speed));
	req.bmRequestType = UT_WRITE_CLASS_ENDPOINT;
	req.bRequest = SET_CUR;
	USETW2(req.wValue, SAMPLING_FREQ_CONTROL, 0);
	USETW(req.wIndex, endpt);
	USETW(req.wLength, 3);
	data[0] = speed;
	data[1] = speed >> 8;
	data[2] = speed >> 16;

	return (usbd_do_request(sc->sc_udev, &req, data));
}

void
uaudio_set_speed_emu0202(struct chan *ch)
{
	usb_device_request_t req;
	int rates[6] = { 44100, 48000, 88200, 96000, 176400, 192000 };
	int i;
	u_int8_t data[1];

	for (i = 0; i < 6; i++)
		if (rates[i] >= ch->sample_rate)
			break;
	if (i >= 6) {
		DPRINTF(("%s: unhandled rate %d\n", __func__, ch->sample_rate));
		i = 0;
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = SET_CUR;
	USETW2(req.wValue, 0x03, 0);
	USETW2(req.wIndex, 12, ch->sc->sc_ac_iface);
	USETW(req.wLength, 1);
	data[0] = i;

	usbd_do_request(ch->sc->sc_udev, &req, data);
}

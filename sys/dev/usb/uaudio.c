/*	$OpenBSD: uaudio.c,v 1.3 2000/03/28 19:37:49 aaron Exp $ */
/*	$NetBSD: uaudio.c,v 1.20 2000/03/24 13:02:00 augustss Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
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

/*
 * USB audio specs: http://www.teleport.com/~usb/data/Audio10.pdf
 *                  http://www.teleport.com/~usb/data/Frmts10.pdf
 *                  http://www.teleport.com/~usb/data/Termt10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uaudioreg.h>

#ifdef UAUDIO_DEBUG
#define DPRINTF(x)	if (uaudiodebug) logprintf x
#define DPRINTFN(n,x)	if (uaudiodebug>(n)) logprintf x
int	uaudiodebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UAUDIO_NCHANBUFS 5	/* number of outstanding request */
#define UAUDIO_NFRAMES   20	/* ms of sound in each request */


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
#define MIX_SIZE(n) ((n) == MIX_SIGNED_16 || (n) == MIX_UNSIGNED_16 ? 2 : 1)
#define MIX_UNSIGNED(n) ((n) == MIX_UNSIGNED_16)
	int		minval, maxval;
	u_int8_t	class;
	char		ctlname[MAX_AUDIO_DEV_LEN];
	char		*ctlunit;
};
#define MAKE(h,l) (((h) << 8) | (l))

struct as_info {
	u_int8_t	alt;
	u_int8_t	encoding;
	usb_interface_descriptor_t *idesc;
	usb_endpoint_descriptor_audio_t *edesc;
	struct usb_audio_streaming_type1_descriptor *asf1desc;
};

struct chan {
	int	terminal;	/* terminal id */
	void	(*intr) __P((void *));	/* dma completion intr handler */
	void	*arg;		/* arg for intr() */
	usbd_pipe_handle pipe;
	int	dir;		/* direction, UE_DIR_XXX */

	u_int	sample_size;
	u_int	sample_rate;
	u_int	bytes_per_frame;
	u_int	fraction;	/* fraction/1000 is the extra samples/frame */
	u_int	residue;	/* accumulates the fractional samples */

	u_char	*start;		/* upper layer buffer start */
	u_char	*end;		/* upper layer buffer end */
	u_char	*cur;		/* current position in upper layer buffer */
	int	blksize;	/* chunk size to report up */
	int	transferred;	/* transferred bytes not reported up */

	int	curchanbuf;
	struct chanbuf {
		struct chan         *chan;
		usbd_xfer_handle xfer;
		u_char              *buffer;
		u_int16_t           sizes[UAUDIO_NFRAMES];
		u_int16_t	    size;
	} chanbufs[UAUDIO_NCHANBUFS];

	struct uaudio_softc *sc; /* our softc */
};

struct uaudio_softc {
	USBBASEDEVICE sc_dev;		/* base device */
	usbd_device_handle sc_udev;	/* USB device */

	int	sc_ac_iface;	/* Audio Control interface */
	int	sc_as_iface;	/* Audio Streaming interface */
	usbd_interface_handle	sc_ac_ifaceh;
	usbd_interface_handle	sc_as_ifaceh;

	struct chan sc_chan;

	int	sc_curaltidx;

	int	sc_nullalt;

	int	sc_audio_rev;

	struct as_info *sc_alts;
	int	sc_nalts;
	int	sc_props;

	int	sc_altflags;
#define HAS_8     0x01
#define HAS_16    0x02
#define HAS_8U    0x04
#define HAS_ALAW  0x08
#define HAS_MULAW 0x10

	struct mixerctl *sc_ctls;
	int	sc_nctls;

	device_ptr_t sc_audiodev;
	char	sc_dying;
};

#define UAC_OUTPUT 0
#define UAC_INPUT  1
#define UAC_EQUAL  2

static usbd_status	uaudio_identify_ac __P((struct uaudio_softc *sc,
			    usb_config_descriptor_t *cdesc));
static usbd_status	uaudio_identify_as __P((struct uaudio_softc *sc,
			    usb_config_descriptor_t *cdesc));
static usbd_status	uaudio_process_as __P((struct uaudio_softc *sc,
			    char *buf, int *offsp, int size,
			    usb_interface_descriptor_t *id));

static void 		uaudio_add_alt __P((struct uaudio_softc *sc, 
			    struct as_info *ai));

static usb_interface_descriptor_t *uaudio_find_iface 
	__P((char *buf, int size, int *offsp, int subtype));

static void		uaudio_mixer_add_ctl __P((struct uaudio_softc *sc,
			    struct mixerctl *mp));
static char 		*uaudio_id_name __P((struct uaudio_softc *sc,
			    usb_descriptor_t **dps, int id));
static struct usb_audio_cluster uaudio_get_cluster __P((int id, 
			    usb_descriptor_t **dps));
static void		uaudio_add_input __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void 		uaudio_add_output __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_mixer __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_selector __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_feature __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_processing_updown
			    __P((struct uaudio_softc *sc,
			         usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_processing __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static void		uaudio_add_extension __P((struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps));
static usbd_status	uaudio_identify __P((struct uaudio_softc *sc, 
			    usb_config_descriptor_t *cdesc));

static int 		uaudio_signext __P((int type, int val));
static int 		uaudio_value2bsd __P((struct mixerctl *mc, int val));
static int 		uaudio_bsd2value __P((struct mixerctl *mc, int val));
static int 		uaudio_get __P((struct uaudio_softc *sc, int type,
			    int which, int wValue, int wIndex, int len));
static int		uaudio_ctl_get __P((struct uaudio_softc *sc, int which,
			    struct mixerctl *mc, int chan));
static void		uaudio_set __P((struct uaudio_softc *sc, int type,
			    int which, int wValue, int wIndex, int l, int v));
static void 		uaudio_ctl_set __P((struct uaudio_softc *sc, int which,
			    struct mixerctl *mc, int chan, int val));

static usbd_status	uaudio_set_speed __P((struct uaudio_softc *, int,
			    u_int));

static usbd_status	uaudio_chan_open __P((struct uaudio_softc *sc,
			    struct chan *ch));
static void		uaudio_chan_close __P((struct uaudio_softc *sc,
			    struct chan *ch));
static usbd_status	uaudio_chan_alloc_buffers __P((struct uaudio_softc *,
			    struct chan *));
static void		uaudio_chan_free_buffers __P((struct uaudio_softc *,
			    struct chan *));
static void		uaudio_chan_set_param __P((struct chan *ch,
			    struct audio_params *param, u_char *start, 
			    u_char *end, int blksize));
static void		uaudio_chan_ptransfer __P((struct chan *ch));
static void		uaudio_chan_pintr __P((usbd_xfer_handle xfer, 
			    usbd_private_handle priv, usbd_status status));

static void		uaudio_chan_rtransfer __P((struct chan *ch));
static void		uaudio_chan_rintr __P((usbd_xfer_handle xfer, 
			    usbd_private_handle priv, usbd_status status));



static int		uaudio_open __P((void *, int));
static void		uaudio_close __P((void *));
static int		uaudio_drain __P((void *));
static int		uaudio_query_encoding __P((void *,
			    struct audio_encoding *));
static int		uaudio_set_params __P((void *, int, int, 
			    struct audio_params *, struct audio_params *));
static int		uaudio_round_blocksize __P((void *, int));
static int		uaudio_trigger_output __P((void *, void *, void *,
			    int, void (*)(void *), void *,
			    struct audio_params *));
static int		uaudio_trigger_input  __P((void *, void *, void *,
			    int, void (*)(void *), void *,
			    struct audio_params *));
static int		uaudio_halt_in_dma __P((void *));
static int		uaudio_halt_out_dma __P((void *));
static int		uaudio_getdev __P((void *, struct audio_device *));
static int		uaudio_mixer_set_port __P((void *, mixer_ctrl_t *));
static int		uaudio_mixer_get_port __P((void *, mixer_ctrl_t *));
static int		uaudio_query_devinfo __P((void *, mixer_devinfo_t *));
static int		uaudio_get_props __P((void *));

static struct audio_hw_if uaudio_hw_if = {
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
};

static struct audio_device uaudio_device = {
	"USB audio",
	"",
	"uaudio"
};

USB_DECLARE_DRIVER(uaudio);

USB_MATCH(uaudio)
{
	USB_MATCH_START(uaudio, uaa);
	usb_interface_descriptor_t *id;
	
	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	/* Trigger on the control interface. */
	if (id == NULL || 
	    id->bInterfaceClass != UICLASS_AUDIO ||
	    id->bInterfaceSubClass != UISUBCLASS_AUDIOCONTROL ||
	    (usbd_get_quirks(uaa->device)->uq_flags & UQ_BAD_AUDIO))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS_IFACESUBCLASS);
}

USB_ATTACH(uaudio)
{
	USB_ATTACH_START(uaudio, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t *cdesc;
	char devinfo[1024];
	usbd_status err;
	int i;

	usbd_devinfo(uaa->device, 0, devinfo);
	printf(": %s\n", devinfo);

	sc->sc_udev = uaa->device;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = uaudio_identify(sc, cdesc);
	if (err) {
		printf("%s: audio descriptors make no sense, error=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ac_ifaceh = uaa->iface;
	/* Pick up the AS interface. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] != NULL) {
			id = usbd_get_interface_descriptor(uaa->ifaces[i]);
			if (id != NULL &&
			    id->bInterfaceNumber == sc->sc_as_iface) {
				sc->sc_as_ifaceh = uaa->ifaces[i];
				uaa->ifaces[i] = NULL;
				break;
			}
		}
	}

	if (sc->sc_as_ifaceh == NULL) {
		printf("%s: missing AS interface(s)\n",USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	printf("%s: streaming interface %d, audio rev %d.%02x\n",
	       USBDEVNAME(sc->sc_dev), sc->sc_as_iface,
	       sc->sc_audio_rev >> 8, sc->sc_audio_rev & 0xff);

	sc->sc_chan.sc = sc;

	DPRINTF(("uaudio_attach: doing audio_attach_mi\n"));
#if defined(__OpenBSD__)
	audio_attach_mi(&uaudio_hw_if, sc, &sc->sc_dev);
#else
	sc->sc_audiodev = audio_attach_mi(&uaudio_hw_if, sc, &sc->sc_dev);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

int
uaudio_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_audiodev)
			rv = config_deactivate(sc->sc_audiodev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uaudio_detach(self, flags)
	device_ptr_t self;
	int flags;
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	int rv = 0;

	/* Wait for outstanding requests to complete. */
	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);

	if (sc->sc_audiodev != NULL)
		rv = config_detach(sc->sc_audiodev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

int
uaudio_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	struct uaudio_softc *sc = addr;
	int flags = sc->sc_altflags;
	int idx;

	if (sc->sc_dying)
		return (EIO);
    
	if (sc->sc_nalts == 0 || flags == 0)
		return (ENXIO);

	idx = fp->index;
	switch (idx) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = flags&HAS_8U ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = flags&HAS_MULAW ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = flags&HAS_ALAW ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = flags&HAS_8 ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
        case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}

usb_interface_descriptor_t *
uaudio_find_iface(buf, size, offsp, subtype)
	char *buf;
	int size;
	int *offsp;
	int subtype;
{
	usb_interface_descriptor_t *d;

	while (*offsp < size) {
		d = (void *)(buf + *offsp);
		*offsp += d->bLength;
		if (d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceClass == UICLASS_AUDIO &&
		    d->bInterfaceSubClass == subtype)
			return (d);
	}
	return (0);
}

void
uaudio_mixer_add_ctl(sc, mc)
	struct uaudio_softc *sc;
	struct mixerctl *mc;
{
	if (sc->sc_nctls == NULL)
		sc->sc_ctls = malloc(sizeof *mc, M_USBDEV, M_NOWAIT);
	else
		sc->sc_ctls = realloc(sc->sc_ctls, 
				      (sc->sc_nctls+1) * sizeof *mc,
				      M_USBDEV, M_NOWAIT);
	if (sc->sc_ctls == NULL) {
		printf("uaudio_mixer_add_ctl: no memory\n");
		return;
	}

	if (mc->type != MIX_ON_OFF) {
		/* Determine min and max values. */
		mc->minval = uaudio_signext(mc->type, 
			uaudio_get(sc, GET_MIN, UT_READ_CLASS_INTERFACE, 
				   mc->wValue[0], mc->wIndex, 
				   MIX_SIZE(mc->type)));
		mc->maxval = 1 + uaudio_signext(mc->type, 
			uaudio_get(sc, GET_MAX, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
	} else {
		mc->minval = 0;
		mc->maxval = 1;
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
uaudio_id_name(sc, dps, id)
	struct uaudio_softc *sc;
	usb_descriptor_t **dps;
	int id;
{
	static char buf[32];
	sprintf(buf, "i%d", id);
	return (buf);
}

struct usb_audio_cluster
uaudio_get_cluster(id, dps)
	int id;
	usb_descriptor_t **dps;
{
	struct usb_audio_cluster r;
	usb_descriptor_t *dp;
	int i;

	for (i = 0; i < 25; i++) { /* avoid infinite loops */
		dp = dps[id];
		if (dp == 0)
			goto bad;
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
#define p ((struct usb_audio_input_terminal *)dp)
			r.bNrChannels = p->bNrChannels;
			USETW(r.wChannelConfig, UGETW(p->wChannelConfig));
			r.iChannelNames = p->iChannelNames;
#undef p
			return (r);
		case UDESCSUB_AC_OUTPUT:
#define p ((struct usb_audio_output_terminal *)dp)
			id = p->bSourceId;
#undef p
			break;
		case UDESCSUB_AC_MIXER:
#define p ((struct usb_audio_mixer_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		case UDESCSUB_AC_SELECTOR:
			/* XXX This is not really right */
#define p ((struct usb_audio_selector_unit *)dp)
			id = p->baSourceId[0];
#undef p
			break;
		case UDESCSUB_AC_FEATURE:
#define p ((struct usb_audio_feature_unit *)dp)
			id = p->bSourceId;
#undef p
			break;
		case UDESCSUB_AC_PROCESSING:
#define p ((struct usb_audio_processing_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		case UDESCSUB_AC_EXTENSION:
#define p ((struct usb_audio_extension_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		default:
			goto bad;
		}
	}
 bad:
	printf("uaudio_get_cluster: bad data\n");
	memset(&r, 0, sizeof r);
	return (r);

}

void
uaudio_add_input(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
#ifdef UAUDIO_DEBUG
	struct usb_audio_input_terminal *d = 
		(struct usb_audio_input_terminal *)v;

	DPRINTFN(2,("uaudio_add_input: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bNrChannels=%d wChannelConfig=%d "
		    "iChannelNames=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bNrChannels, UGETW(d->wChannelConfig),
		    d->iChannelNames, d->iTerminal));
#endif
}

void
uaudio_add_output(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
#ifdef UAUDIO_DEBUG
	struct usb_audio_output_terminal *d = 
		(struct usb_audio_output_terminal *)v;

	DPRINTFN(2,("uaudio_add_output: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bSourceId=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bSourceId, d->iTerminal));
#endif
}

void
uaudio_add_mixer(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
	struct usb_audio_mixer_unit *d = (struct usb_audio_mixer_unit *)v;
	struct usb_audio_mixer_unit_1 *d1;
	int c, chs, ichs, ochs, i, o, bno, p, mo, mc, k;
	uByte *bm;
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_mixer: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));
	
	/* Compute the number of input channels */
	ichs = 0;
	for (i = 0; i < d->bNrInPins; i++)
		ichs += uaudio_get_cluster(d->baSourceId[i], dps).bNrChannels;

	/* and the number of output channels */
	d1 = (struct usb_audio_mixer_unit_1 *)&d->baSourceId[d->bNrInPins];
	ochs = d1->bNrChannels;
	DPRINTFN(2,("uaudio_add_mixer: ichs=%d ochs=%d\n", ichs, ochs));

	bm = d1->bmControls;
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.class = -1;
	mix.type = MIX_SIGNED_16;
	mix.ctlunit = AudioNvolume;
#define BIT(bno) ((bm[bno / 8] >> (7 - bno % 8)) & 1)
	for (p = i = 0; i < d->bNrInPins; i++) {
		chs = uaudio_get_cluster(d->baSourceId[i], dps).bNrChannels;
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
			sprintf(mix.ctlname, "mix%d-%s", d->bUnitId,
				uaudio_id_name(sc, dps, d->baSourceId[i]));
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
uaudio_add_selector(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
#ifdef UAUDIO_DEBUG
	struct usb_audio_selector_unit *d =
		(struct usb_audio_selector_unit *)v;

	DPRINTFN(2,("uaudio_add_selector: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));
#endif
	printf("uaudio_add_selector: NOT IMPLEMENTED\n");
}

void
uaudio_add_feature(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
	struct usb_audio_feature_unit *d = (struct usb_audio_feature_unit *)v;
	uByte *ctls = d->bmaControls;
	int ctlsize = d->bControlSize;
	int nchan = (d->bLength - 7) / ctlsize;
	int srcId = d->bSourceId;
	u_int fumask, mmask, cmask;
	struct mixerctl mix;
	int chan, ctl, i, unit;

#define GET(i) (ctls[(i)*ctlsize] | \
		(ctlsize > 1 ? ctls[(i)*ctlsize+1] << 8 : 0))

	mmask = GET(0);
	/* Figure out what we can control */
	for (cmask = 0, chan = 1; chan < nchan; chan++) {
		DPRINTFN(9,("uaudio_add_feature: chan=%d mask=%x\n",
			    chan, GET(chan)));
		cmask |= GET(chan);
	}

	DPRINTFN(1,("uaudio_add_feature: bUnitId=%d bSourceId=%d, "
		    "%d channels, mmask=0x%04x, cmask=0x%04x\n", 
		    d->bUnitId, srcId, nchan, mmask, cmask));

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
		mix.class = -1;	/* XXX */
		switch (ctl) {
		case MUTE_CONTROL:
			mix.type = MIX_ON_OFF;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmute);
			mix.ctlunit = "";
			break;
		case VOLUME_CONTROL:
			mix.type = MIX_SIGNED_16;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmaster);
			mix.ctlunit = AudioNvolume;
			break;
		case BASS_CONTROL:
			mix.type = MIX_SIGNED_8;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNbass);
			mix.ctlunit = AudioNbass;
			break;
		case MID_CONTROL:
			mix.type = MIX_SIGNED_8;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmid);
			mix.ctlunit = AudioNmid;
			break;
		case TREBLE_CONTROL:
			mix.type = MIX_SIGNED_8;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNtreble);
			mix.ctlunit = AudioNtreble;
			break;
		case GRAPHIC_EQUALIZER_CONTROL:
			continue; /* XXX don't add anything */
			break;
		case AGC_CONTROL:
			mix.type = MIX_ON_OFF;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNagc);
			mix.ctlunit = "";
			break;
		case DELAY_CONTROL:
			mix.type = MIX_UNSIGNED_16;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNdelay);
			mix.ctlunit = "4 ms";
			break;
		case BASS_BOOST_CONTROL:
			mix.type = MIX_ON_OFF;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNbassboost);
			mix.ctlunit = "";
			break;
		case LOUDNESS_CONTROL:
			mix.type = MIX_ON_OFF;
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNloudness);
			mix.ctlunit = "";
			break;
		}
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

void
uaudio_add_processing_updown(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
	struct usb_audio_processing_unit *d = 
	    (struct usb_audio_processing_unit *)v;
	struct usb_audio_processing_unit_1 *d1 =
	    (struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	struct usb_audio_processing_unit_updown *ud =
	    (struct usb_audio_processing_unit_updown *)
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
	mix.class = -1;
	mix.type = MIX_ON_OFF;	/* XXX */
	mix.ctlunit = "";
	sprintf(mix.ctlname, "pro%d-mode", d->bUnitId);

	for (i = 0; i < ud->bNrModes; i++) {
		DPRINTFN(2,("uaudio_add_processing_updown: i=%d bm=0x%x\n",
			    i, UGETW(ud->waModes[i])));
		/* XXX */
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

void
uaudio_add_processing(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
	struct usb_audio_processing_unit *d = 
	    (struct usb_audio_processing_unit *)v;
	struct usb_audio_processing_unit_1 *d1 =
	    (struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	int ptype = UGETW(d->wProcessType);
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_processing: wProcessType=%d bUnitId=%d "
		    "bNrInPins=%d\n", ptype, d->bUnitId, d->bNrInPins));

	if (d1->bmControls[0] & UA_PROC_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(XX_ENABLE_CONTROL, 0);
		mix.class = -1;
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		sprintf(mix.ctlname, "pro%d.%d-enable", d->bUnitId, ptype);
		uaudio_mixer_add_ctl(sc, &mix);
	}

	switch(ptype) {
	case UPDOWNMIX_PROCESS:
		uaudio_add_processing_updown(sc, v, dps);
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
uaudio_add_extension(sc, v, dps)
	struct uaudio_softc *sc;
	usb_descriptor_t *v;
	usb_descriptor_t **dps;
{
	struct usb_audio_extension_unit *d = 
	    (struct usb_audio_extension_unit *)v;
	struct usb_audio_extension_unit_1 *d1 =
	    (struct usb_audio_extension_unit_1 *)&d->baSourceId[d->bNrInPins];
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_extension: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));

	if (d1->bmControls[0] & UA_EXT_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(UA_EXT_ENABLE, 0);
		mix.class = -1;
		mix.type = MIX_ON_OFF;
		mix.ctlunit = "";
		sprintf(mix.ctlname, "ext%d-enable", d->bUnitId);
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

usbd_status
uaudio_identify(sc, cdesc)
	struct uaudio_softc *sc;
	usb_config_descriptor_t *cdesc;
{
	usbd_status err;

	err = uaudio_identify_ac(sc, cdesc);
	if (err)
		return (err);
	return (uaudio_identify_as(sc, cdesc));
}

void
uaudio_add_alt(sc, ai)
	struct uaudio_softc *sc;
	struct as_info *ai;
{
	if (sc->sc_nalts == NULL)
		sc->sc_alts = malloc(sizeof *ai, M_USBDEV, M_NOWAIT);
	else
		sc->sc_alts = realloc(sc->sc_alts,
				      (sc->sc_nalts+1) * sizeof *ai,
				      M_USBDEV, M_NOWAIT);
	if (sc->sc_alts == NULL) {
		printf("uaudio_add_alt: no memory\n");
		return;
	}
	DPRINTFN(2,("uaudio_add_alt: adding alt=%d, enc=%d\n",
		    ai->alt, ai->encoding));
	sc->sc_alts[sc->sc_nalts++] = *ai;
}

usbd_status
uaudio_process_as(sc, buf, offsp, size, id)
	struct uaudio_softc *sc;
	char *buf;
	int *offsp;
#define offs (*offsp)
	int size;
	usb_interface_descriptor_t *id;
{
	struct usb_audio_streaming_interface_descriptor *asid;
	struct usb_audio_streaming_type1_descriptor *asf1d;
	usb_endpoint_descriptor_audio_t *ed;
	struct usb_audio_streaming_endpoint_descriptor *sed;
	int format, chan, prec, enc;
	int dir, type;
	struct as_info ai;

	asid = (void *)(buf + offs);
	if (asid->bDescriptorType != UDESC_CS_INTERFACE ||
	    asid->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	offs += asid->bLength;
	if (offs > size)
		return (USBD_INVAL);
	asf1d = (void *)(buf + offs);
	if (asf1d->bDescriptorType != UDESC_CS_INTERFACE ||
	    asf1d->bDescriptorSubtype != FORMAT_TYPE)
		return (USBD_INVAL);
	offs += asf1d->bLength;
	if (offs > size)
		return (USBD_INVAL);

	if (asf1d->bFormatType != FORMAT_TYPE_I) {
		printf("%s: ignored setting with type %d format\n",
		       USBDEVNAME(sc->sc_dev), UGETW(asid->wFormatTag));
		return (USBD_NORMAL_COMPLETION);
	}

	ed = (void *)(buf + offs);
	if (ed->bDescriptorType != UDESC_ENDPOINT)
		return (USBD_INVAL);
	DPRINTF(("uaudio_process_as: endpoint bLength=%d bDescriptorType=%d "
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
	/* We can't handle endpoints that need a sync pipe. */
	if (dir == UE_DIR_IN ? type == UE_ISO_ADAPT : type == UE_ISO_ASYNC) {
		printf("%s: ignored %sput endpoint of type 0x%x\n",
		       USBDEVNAME(sc->sc_dev),
		       dir == UE_DIR_IN ? "in" : "out",
		       ed->bmAttributes & UE_ISO_TYPE);
		return (USBD_NORMAL_COMPLETION);
	}
	
	sed = (void *)(buf + offs);
	if (sed->bDescriptorType != UDESC_CS_ENDPOINT ||
	    sed->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	offs += sed->bLength;
	if (offs > size)
		return (USBD_INVAL);
	
	format = UGETW(asid->wFormatTag);
	chan = asf1d->bNrChannels;
	prec = asf1d->bBitResolution;
	if (prec != 8 && prec != 16) {
#ifdef AUDIO_DEBUG
		printf("%s: ignored setting with precision %d\n",
		       USBDEVNAME(sc->sc_dev), prec);
#endif
		return (USBD_NORMAL_COMPLETION);
	}
	switch (format) {
	case UA_FMT_PCM:
		sc->sc_altflags |= prec == 8 ? HAS_8 : HAS_16;
		enc = AUDIO_ENCODING_SLINEAR_LE;
		break;
	case UA_FMT_PCM8:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		sc->sc_altflags |= HAS_8U;
		break;
	case UA_FMT_ALAW:
		enc = AUDIO_ENCODING_ALAW;
		sc->sc_altflags |= HAS_ALAW;
		break;
	case UA_FMT_MULAW:
		enc = AUDIO_ENCODING_ULAW;
		sc->sc_altflags |= HAS_MULAW;
		break;
	default:
		printf("%s: ignored setting with format %d\n",
		       USBDEVNAME(sc->sc_dev), format);
		return (USBD_NORMAL_COMPLETION);
	}
	DPRINTFN(1,("uaudio_identify: alt=%d enc=%d chan=%d prec=%d\n",
		    id->bAlternateSetting, enc, chan, prec));
	ai.alt = id->bAlternateSetting;
	ai.encoding = enc;
	ai.idesc = id;
	ai.edesc = ed;
	ai.asf1desc = asf1d;
	uaudio_add_alt(sc, &ai);
	sc->sc_chan.terminal = asid->bTerminalLink; /* XXX */
	sc->sc_chan.dir = dir;
	return (USBD_NORMAL_COMPLETION);
}
#undef offs
	
usbd_status
uaudio_identify_as(sc, cdesc)
	struct uaudio_softc *sc;
	usb_config_descriptor_t *cdesc;
{
	usb_interface_descriptor_t *id;
	usbd_status err;
	char *buf;
	int size, offs;

	size = UGETW(cdesc->wTotalLength);
	buf = (char *)cdesc;

	/* Locate the AudioStreaming interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOSTREAM);
	if (id == NULL)
		return (USBD_INVAL);
	sc->sc_as_iface = id->bInterfaceNumber;
	DPRINTF(("uaudio_identify_as: AS interface is %d\n", sc->sc_as_iface));

	sc->sc_chan.terminal = -1;

	/* Loop through all the alternate settings. */
	while (offs <= size) {
		switch (id->bNumEndpoints) {
		case 0:
			DPRINTFN(2, ("uaudio_identify: AS null alt=%d\n",
				     id->bAlternateSetting));
			sc->sc_nullalt = id->bAlternateSetting;
			break;
		case 1:
			err = uaudio_process_as(sc, buf, &offs, size, id);
			break;
		default:
#ifdef AUDIO_DEBUG
			printf("%s: ignored audio interface with %d "
			       "endpoints\n",
			       USBDEVNAME(sc->sc_dev), id->bNumEndpoints);
#endif
			break;
		}
		id = uaudio_find_iface(buf, size, &offs,UISUBCLASS_AUDIOSTREAM);
		if (id == NULL)
			break;
	}
	if (offs > size)
		return (USBD_INVAL);
	DPRINTF(("uaudio_identify_as: %d alts available\n", sc->sc_nalts));
	if (sc->sc_chan.terminal < 0) {
		printf("%s: no useable endpoint found\n", 
		       USBDEVNAME(sc->sc_dev));
		return (USBD_INVAL);
	}
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uaudio_identify_ac(sc, cdesc)
	struct uaudio_softc *sc;
	usb_config_descriptor_t *cdesc;
{
	usb_interface_descriptor_t *id;
	struct usb_audio_control_descriptor *acdp;
	usb_descriptor_t *dp, *dps[256];
	char *buf, *ibuf, *ibufend;
	int size, offs, aclen, ndps, i;

	size = UGETW(cdesc->wTotalLength);
	buf = (char *)cdesc;

	/* Locate the AudioControl interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, UISUBCLASS_AUDIOCONTROL);
	if (id == NULL)
		return (USBD_INVAL);
	if (offs + sizeof *acdp > size)
		return (USBD_INVAL);
	sc->sc_ac_iface = id->bInterfaceNumber;
	DPRINTFN(2,("uaudio_identify: AC interface is %d\n", sc->sc_ac_iface));

	/* A class-specific AC interface header should follow. */
	ibuf = buf + offs;
	acdp = (struct usb_audio_control_descriptor *)ibuf;
	if (acdp->bDescriptorType != UDESC_CS_INTERFACE ||
	    acdp->bDescriptorSubtype != UDESCSUB_AC_HEADER)
		return (USBD_INVAL);
	aclen = UGETW(acdp->wTotalLength);
	if (offs + aclen > size)
		return (USBD_INVAL);

	if (!(usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_BAD_ADC) &&
	     UGETW(acdp->bcdADC) != UAUDIO_VERSION)
		return (USBD_INVAL);

	sc->sc_audio_rev = UGETW(acdp->bcdADC);
	DPRINTFN(2,("uaudio_identify: found AC header, vers=%03x, len=%d\n",
		 sc->sc_audio_rev, aclen));

	sc->sc_nullalt = -1;

	/* Scan through all the AC specific descriptors */
	ibufend = ibuf + aclen;
	dp = (usb_descriptor_t *)ibuf;
	ndps = 0;
	memset(dps, 0, sizeof dps);
	for (;;) {
		ibuf += dp->bLength;
		if (ibuf >= ibufend)
			break;
		dp = (usb_descriptor_t *)ibuf;
		if (ibuf + dp->bLength > ibufend)
			return (USBD_INVAL);
		if (dp->bDescriptorType != UDESC_CS_INTERFACE) {
			printf("uaudio_identify: skip desc type=0x%02x\n",
			       dp->bDescriptorType);
			continue;
		}
		i = ((struct usb_audio_input_terminal *)dp)->bTerminalId;
		dps[i] = dp;
		if (i > ndps)
			ndps = i;
	}
	ndps++;

	for (i = 0; i < ndps; i++) {
		dp = dps[i];
		if (dp == NULL)
			continue;
		DPRINTF(("uaudio_identify: subtype=%d\n", 
			 dp->bDescriptorSubtype));
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_HEADER:
			printf("uaudio_identify: unexpected AC header\n");
			break;
		case UDESCSUB_AC_INPUT:
			uaudio_add_input(sc, dp, dps);
			break;
		case UDESCSUB_AC_OUTPUT:
			uaudio_add_output(sc, dp, dps);
			break;
		case UDESCSUB_AC_MIXER:
			uaudio_add_mixer(sc, dp, dps);
			break;
		case UDESCSUB_AC_SELECTOR:
			uaudio_add_selector(sc, dp, dps);
			break;
		case UDESCSUB_AC_FEATURE:
			uaudio_add_feature(sc, dp, dps);
			break;
		case UDESCSUB_AC_PROCESSING:
			uaudio_add_processing(sc, dp, dps);
			break;
		case UDESCSUB_AC_EXTENSION:
			uaudio_add_extension(sc, dp, dps);
			break;
		default:
			printf("uaudio_identify: bad AC desc subtype=0x%02x\n",
			       dp->bDescriptorSubtype);
			break;
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

int
uaudio_query_devinfo(addr, mi)
	void *addr;
	mixer_devinfo_t *mi;
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int n, nctls;

	DPRINTFN(2,("uaudio_query_devinfo: index=%d\n", mi->index));
	if (sc->sc_dying)
		return (EIO);
    
	n = mi->index;
	nctls = sc->sc_nctls;

	if (n < 0 || n >= nctls) {
		switch (n - nctls) {
		case UAC_OUTPUT:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_OUTPUT;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCoutputs);
			return (0);
		case UAC_INPUT:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_INPUT;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCinputs);
			return (0);
		case UAC_EQUAL:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_EQUAL;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCequalization);
			return (0);
		default:
			return (ENXIO);
		}
	}
	mc = &sc->sc_ctls[n];
	strncpy(mi->label.name, mc->ctlname, MAX_AUDIO_DEV_LEN);
	mi->mixer_class = mc->class;
	mi->next = mi->prev = AUDIO_MIXER_LAST;	/* XXX */
	switch (mc->type) {
	case MIX_ON_OFF:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = 2;
		strcpy(mi->un.e.member[0].label.name, AudioNoff);
		mi->un.e.member[0].ord = 0;
		strcpy(mi->un.e.member[1].label.name, AudioNon);
		mi->un.e.member[1].ord = 1;
		break;
	default:
		mi->type = AUDIO_MIXER_VALUE;
		strncpy(mi->un.v.units.name, mc->ctlunit, MAX_AUDIO_DEV_LEN);
		mi->un.v.num_channels = mc->nchan;
		break;
	}
	return (0);
}

int
uaudio_open(addr, flags)
	void *addr;
	int flags;
{
	struct uaudio_softc *sc = addr;

        DPRINTF(("uaudio_open: sc=%p\n", sc));
	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_chan.terminal < 0)
		return (ENXIO);

	if ((flags & FREAD) && sc->sc_chan.dir != UE_DIR_IN)
		return (EACCES);
	if ((flags & FWRITE) && sc->sc_chan.dir != UE_DIR_OUT)
		return (EACCES);

        sc->sc_chan.intr = 0;

        return (0);
}

/*
 * Close function is called at splaudio().
 */
void
uaudio_close(addr)
	void *addr;
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_close: sc=%p\n", sc));
	uaudio_halt_in_dma(sc);
	uaudio_halt_out_dma(sc);

	sc->sc_chan.intr = 0;
}

int
uaudio_drain(addr)
	void *addr;
{
	struct uaudio_softc *sc = addr;

	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);

	return (0);
}

int
uaudio_halt_out_dma(addr)
	void *addr;
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_halt_out_dma: enter\n"));
	if (sc->sc_chan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_chan);
		sc->sc_chan.pipe = 0;
		uaudio_chan_free_buffers(sc, &sc->sc_chan);
	}
        return (0);
}

int
uaudio_halt_in_dma(addr)
	void *addr;
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_halt_in_dma: enter\n"));
	if (sc->sc_chan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_chan);
		sc->sc_chan.pipe = 0;
		uaudio_chan_free_buffers(sc, &sc->sc_chan);
	}
        return (0);
}

int
uaudio_getdev(addr, retp)
	void *addr;
        struct audio_device *retp;
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_mixer_getdev:\n"));
	if (sc->sc_dying)
		return (EIO);
    
	*retp = uaudio_device;
        return (0);
}

/*
 * Make sure the block size is large enough to hold all outstanding transfers.
 */
int
uaudio_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	struct uaudio_softc *sc = addr;
	int bpf;

	bpf = sc->sc_chan.bytes_per_frame + sc->sc_chan.sample_size;
	/* XXX */
	bpf *= UAUDIO_NFRAMES * UAUDIO_NCHANBUFS;

	bpf = (bpf + 15) &~ 15;

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
uaudio_get_props(addr)
	void *addr;
{
	struct uaudio_softc *sc = addr;

	return (sc->sc_props);
}

int
uaudio_get(sc, which, type, wValue, wIndex, len)
	struct uaudio_softc *sc;
	int type, which, wValue, wIndex, len;
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
	err = usbd_do_request(sc->sc_udev, &req, &data);
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
uaudio_set(sc, which, type, wValue, wIndex, len, val)
	struct uaudio_softc *sc;
	int type, which, wValue, wIndex, len, val;
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
	err = usbd_do_request(sc->sc_udev, &req, &data);
#ifdef UAUDIO_DEBUG
	if (err)
		DPRINTF(("uaudio_set: err=%d\n", err));
#endif
}

int
uaudio_signext(type, val)
	int type, val;
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
uaudio_value2bsd(mc, val)
	struct mixerctl *mc;
	int val;
{
	DPRINTFN(5, ("uaudio_value2bsd: type=%03x val=%d min=%d max=%d ",
		     mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF)
		val = val != 0;
	else
		val = (uaudio_signext(mc->type, val) - mc->minval) * 256
			/ (mc->maxval - mc->minval);
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}

int
uaudio_bsd2value(mc, val)
	struct mixerctl *mc;
	int val;
{
	DPRINTFN(5,("uaudio_bsd2value: type=%03x val=%d min=%d max=%d ",
		    mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF)
		val = val != 0;
	else
		val = val * (mc->maxval - mc->minval) / 256 + mc->minval;
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}

int
uaudio_ctl_get(sc, which, mc, chan)
	struct uaudio_softc *sc;
	int which;
	struct mixerctl *mc;
	int chan;
{
	int val;

	DPRINTFN(5,("uaudio_ctl_get: which=%d chan=%d\n", which, chan));
	val = uaudio_get(sc, which, UT_READ_CLASS_INTERFACE, mc->wValue[chan],
			 mc->wIndex, MIX_SIZE(mc->type));
	return (uaudio_value2bsd(mc, val));
}

void
uaudio_ctl_set(sc, which, mc, chan, val)
	struct uaudio_softc *sc;
	int which;
	struct mixerctl *mc;
	int chan;
	int val;
{
	val = uaudio_bsd2value(mc, val);
	uaudio_set(sc, which, UT_WRITE_CLASS_INTERFACE, mc->wValue[chan],
		   mc->wIndex, MIX_SIZE(mc->type), val);
}

int
uaudio_mixer_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN], val;

	DPRINTFN(2,("uaudio_mixer_get_port: index=%d\n", cp->dev));

	if (sc->sc_dying)
		return (EIO);
    
	n = cp->dev;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
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
uaudio_mixer_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN];

	DPRINTFN(2,("uaudio_mixer_set_port: index = %d\n", cp->dev));
	if (sc->sc_dying)
		return (EIO);
    
	n = cp->dev;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
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
uaudio_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_chan;
	usbd_status err;
	int i, s;

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_input: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, param, start, end, blksize);
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

	sc->sc_chan.intr = intr;
	sc->sc_chan.arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		uaudio_chan_rtransfer(ch);
	splx(s);

        return (0);
}
    
int
uaudio_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_chan;
	usbd_status err;
	int i, s;

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_output: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, param, start, end, blksize);
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

	sc->sc_chan.intr = intr;
	sc->sc_chan.arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		uaudio_chan_ptransfer(ch);
	splx(s);

        return (0);
}

/* Set up a pipe for a channel. */
usbd_status
uaudio_chan_open(sc, ch)
	struct uaudio_softc *sc;
	struct chan *ch;
{
	struct as_info *as = &sc->sc_alts[sc->sc_curaltidx];
	int endpt = as->edesc->bEndpointAddress;
	usbd_status err;

	DPRINTF(("uaudio_open_chan: endpt=0x%02x, speed=%d, alt=%d\n", 
		 endpt, ch->sample_rate, as->alt));

	/* Set alternate interface corresponding to the mode. */
	err = usbd_set_interface(sc->sc_as_ifaceh, as->alt);
	if (err)
		return (err);

	/* Some devices do not support this request, so ignore errors. */
#ifdef UAUDIO_DEBUG
	err = uaudio_set_speed(sc, endpt, ch->sample_rate);
	if (err)
		DPRINTF(("uaudio_chan_open: set_speed failed err=%s\n",
			 usbd_errstr(err)));
#else
	(void)uaudio_set_speed(sc, endpt, ch->sample_rate);
#endif

	DPRINTF(("uaudio_open_chan: create pipe to 0x%02x\n", endpt));
	err = usbd_open_pipe(sc->sc_as_ifaceh, endpt, 0, &ch->pipe);
	return (err);
}

void
uaudio_chan_close(sc, ch)
	struct uaudio_softc *sc;
	struct chan *ch;
{
	if (sc->sc_nullalt >= 0) {
		DPRINTF(("uaudio_close_chan: set null alt=%d\n",
			 sc->sc_nullalt));
		usbd_set_interface(sc->sc_as_ifaceh, sc->sc_nullalt);
	}
	usbd_abort_pipe(ch->pipe);
	usbd_close_pipe(ch->pipe);
}

usbd_status
uaudio_chan_alloc_buffers(sc, ch)
	struct uaudio_softc *sc;
	struct chan *ch;
{
	usbd_xfer_handle xfer;
	void *buf;
	int i, size;

	size = (ch->bytes_per_frame + ch->sample_size) * UAUDIO_NFRAMES;
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

	return (USBD_NORMAL_COMPLETION);

bad:
	while (--i >= 0)
		/* implicit buffer free */
		usbd_free_xfer(ch->chanbufs[i].xfer);
	return (USBD_NOMEM);
}

void
uaudio_chan_free_buffers(sc, ch)
	struct uaudio_softc *sc;
	struct chan *ch;
{
	int i;

	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		usbd_free_xfer(ch->chanbufs[i].xfer);
}

/* Called at splusb() */
void
uaudio_chan_ptransfer(ch)
	struct chan *ch;
{
	struct chanbuf *cb;
	int i, n, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= USB_FRAMES_PER_SECOND) {
			size += ch->sample_size;
			residue -= USB_FRAMES_PER_SECOND;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

	/* 
	 * Transfer data from upper layer buffer to channel buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	n = min(total, ch->end - ch->cur);
	memcpy(cb->buffer, ch->cur, n);
	ch->cur += n;
	if (ch->cur >= ch->end)
		ch->cur = ch->start;
	if (total > n) {
		total -= n;
		memcpy(cb->buffer + n, ch->cur, total);
		ch->cur += total;
	}

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_ptransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_transfer: ptransfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes, 
			     UAUDIO_NFRAMES, USBD_NO_COPY, 
			     uaudio_chan_pintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_pintr(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int32_t count;
	int s;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_pintr: count=%d, transferred=%d\n",
		    count, ch->transferred));
#ifdef DIAGNOSTIC
	if (count != cb->size) {
		printf("uaudio_chan_pintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	ch->transferred += cb->size;
	s = splaudio();
	/* Call back to upper layer */
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5,("uaudio_chan_pintr: call %p(%p)\n", 
			    ch->intr, ch->arg));
		ch->intr(ch->arg);
	}
	splx(s);

	/* start next transfer */
	uaudio_chan_ptransfer(ch);
}

/* Called at splusb() */
void
uaudio_chan_rtransfer(ch)
	struct chan *ch;
{
	struct chanbuf *cb;
	int i, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= USB_FRAMES_PER_SECOND) {
			size += ch->sample_size;
			residue -= USB_FRAMES_PER_SECOND;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

#ifdef UAUDIO_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_rtransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_rtransfer: transfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes, 
			     UAUDIO_NFRAMES, USBD_NO_COPY, 
			     uaudio_chan_rintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_rintr(xfer, priv, status)
	usbd_xfer_handle xfer;
	usbd_private_handle priv;
	usbd_status status;
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int32_t count;
	int s, n;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_rintr: count=%d, transferred=%d\n",
		    count, ch->transferred));
#ifdef DIAGNOSTIC
	if (count != cb->size) {
		printf("uaudio_chan_pintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	/* 
	 * Transfer data from channel buffer to upper layer buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	n = min(count, ch->end - ch->cur);
	memcpy(ch->cur, cb->buffer, n);
	ch->cur += n;
	if (ch->cur >= ch->end)
		ch->cur = ch->start;
	if (count > n) {
		memcpy(ch->cur, cb->buffer + n, count - n);
		ch->cur += count - n;
	}

	/* Call back to upper layer */
	ch->transferred += cb->size;
	s = splaudio();
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5,("uaudio_chan_pintr: call %p(%p)\n", 
			    ch->intr, ch->arg));
		ch->intr(ch->arg);
	}
	splx(s);

	/* start next transfer */
	uaudio_chan_rtransfer(ch);
}

void
uaudio_chan_set_param(ch, param, start, end, blksize)
	struct chan *ch;
	struct audio_params *param;
	u_char *start, *end;
	int blksize;
{
	int samples_per_frame, sample_size;

	sample_size = param->precision * param->channels / 8;
	samples_per_frame = param->sample_rate / USB_FRAMES_PER_SECOND;
	ch->fraction = param->sample_rate % USB_FRAMES_PER_SECOND;
	ch->sample_size = sample_size;
	ch->sample_rate = param->sample_rate;
	ch->bytes_per_frame = samples_per_frame * sample_size;
	ch->residue = 0;

	ch->start = start;
	ch->end = end;
	ch->cur = start;
	ch->blksize = blksize;
	ch->transferred = 0;

	ch->curchanbuf = 0;
}

int
uaudio_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct uaudio_softc *sc = addr;
	int flags = sc->sc_altflags;
	int pfactor, rfactor;
	int enc, i, j;
	void (*pswcode) __P((void *, u_char *buf, int cnt));
	void (*rswcode) __P((void *, u_char *buf, int cnt));

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_chan.pipe != NULL)
		return (EBUSY);

        pswcode = rswcode = 0;
	pfactor = rfactor = 1;
	enc = p->encoding;
        switch (p->encoding) {
        case AUDIO_ENCODING_SLINEAR_BE:
        	if (p->precision == 16) {
                	rswcode = pswcode = swap_bytes;
			enc = AUDIO_ENCODING_SLINEAR_LE;
		} else if (p->precision == 8 && !(flags & HAS_8)) {
			pswcode = rswcode = change_sign8;
			enc = AUDIO_ENCODING_ULINEAR_LE;
		}
		break;
        case AUDIO_ENCODING_SLINEAR_LE:
        	if (p->precision == 8 && !(flags & HAS_8)) {
			pswcode = rswcode = change_sign8;
			enc = AUDIO_ENCODING_ULINEAR_LE;
		}
        	break;
        case AUDIO_ENCODING_ULINEAR_BE:
        	if (p->precision == 16) {
			pswcode = swap_bytes_change_sign16_le;
			rswcode = change_sign16_swap_bytes_le;
			enc = AUDIO_ENCODING_SLINEAR_LE;
		} else if (p->precision == 8 && !(flags & HAS_8U)) {
			pswcode = rswcode = change_sign8;
			enc = AUDIO_ENCODING_SLINEAR_LE;
		}
		break;
        case AUDIO_ENCODING_ULINEAR_LE:
        	if (p->precision == 16) {
			pswcode = rswcode = change_sign16_le;
			enc = AUDIO_ENCODING_SLINEAR_LE;
		} else if (p->precision == 8 && !(flags & HAS_8U)) {
			pswcode = rswcode = change_sign8;
			enc = AUDIO_ENCODING_SLINEAR_LE;
		}
        	break;
        case AUDIO_ENCODING_ULAW:
		if (!(flags & HAS_MULAW)) {
			if (flags & HAS_8U) {
				pswcode = mulaw_to_ulinear8;
				rswcode = ulinear8_to_mulaw;
				enc = AUDIO_ENCODING_ULINEAR_LE;
			} else if (flags & HAS_8) {
				pswcode = mulaw_to_slinear8;
				rswcode = slinear8_to_mulaw;
				enc = AUDIO_ENCODING_SLINEAR_LE;
#if 0
			} else if (flags & HAS_16) {
				pswcode = mulaw_to_slinear16_le;
				pfactor = 2;
				/* XXX recording not handled */
				enc = AUDIO_ENCODING_SLINEAR_LE;
#endif
			} else
				return (EINVAL);
		}
                break;
        case AUDIO_ENCODING_ALAW:
		if (!(flags & HAS_ALAW)) {
			if (flags & HAS_8U) {
				pswcode = alaw_to_ulinear8;
				rswcode = ulinear8_to_alaw;
				enc = AUDIO_ENCODING_ULINEAR_LE;
			} else if (flags & HAS_8) {
				pswcode = alaw_to_slinear8;
				rswcode = slinear8_to_alaw;
				enc = AUDIO_ENCODING_SLINEAR_LE;
#if 0
			} else if (flags & HAS_16) {
				pswcode = alaw_to_slinear16_le;
				pfactor = 2;
				/* XXX recording not handled */
				enc = AUDIO_ENCODING_SLINEAR_LE;
#endif
			} else
				return (EINVAL);
		}
                break;
        default:
        	return (EINVAL);
        }
	/* XXX do some other conversions... */

	DPRINTF(("uaudio_set_params: chan=%d prec=%d enc=%d rate=%ld\n",
		 p->channels, p->precision, enc, p->sample_rate));

	for (i = 0; i < sc->sc_nalts; i++) {
		struct usb_audio_streaming_type1_descriptor *a1d =
			sc->sc_alts[i].asf1desc;
		if (p->channels == a1d->bNrChannels &&
		    p->precision ==a1d->bBitResolution &&
		    enc == sc->sc_alts[i].encoding) {
			if (a1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
				DPRINTFN(2,("uaudio_set_params: cont %d-%d\n",
				    UA_SAMP_LO(a1d), UA_SAMP_HI(a1d)));
				if (UA_SAMP_LO(a1d) < p->sample_rate &&
				    p->sample_rate < UA_SAMP_HI(a1d))
					goto found;
			} else {
				for (j = 0; j < a1d->bSamFreqType; j++) {
					DPRINTFN(2,("uaudio_set_params: disc #"
					    "%d: %d\n", j, UA_GETSAMP(a1d, j)));
					/* XXX allow for some slack */
					if (UA_GETSAMP(a1d, j) ==
					    p->sample_rate)
						goto found;
				}
			}
		}
	}
	return (EINVAL);

 found:
        p->sw_code = pswcode;
        r->sw_code = rswcode;
	p->factor  = pfactor;
	r->factor  = rfactor;
	sc->sc_curaltidx = i;

	DPRINTF(("uaudio_set_params: use altidx=%d, altno=%d\n", 
		 sc->sc_curaltidx, 
		 sc->sc_alts[sc->sc_curaltidx].idesc->bAlternateSetting));
	
        return (0);
}

usbd_status
uaudio_set_speed(sc, endpt, speed)
	struct uaudio_softc *sc;
	int endpt;
	u_int speed;
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

	return (usbd_do_request(sc->sc_udev, &req, &data));
}


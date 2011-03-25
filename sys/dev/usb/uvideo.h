/*	$OpenBSD: uvideo.h,v 1.52 2011/03/25 21:23:54 jakemsr Exp $ */

/*
 * Copyright (c) 2007 Robert Nagy <robert@openbsd.org>
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

#include <sys/queue.h>
#include <sys/videoio.h>

/*
 * USB Video Class
 */
/* Table A-7: Video Class-Specific Endpoint Descriptor Subtypes */
#define	EP_UNDEFINED					0x00
#define EP_GENERAL					0x01
#define EP_ENDPOINT					0x02
#define EP_INTERRUPT					0x03

/* Table A-5: Video Class-Specific VC Interface Descriptor Subtypes */
#define	UDESCSUB_VC_DESCRIPTOR_UNDEFINED		0x00
#define UDESCSUB_VC_HEADER				0x01
#define UDESCSUB_VC_INPUT_TERMINAL			0x02
#define UDESCSUB_VC_OUTPUT_TERMINAL			0x03
#define UDESCSUB_VC_SELECTOR_UNIT			0x04
#define UDESCSUB_VC_PROCESSING_UNIT			0x05
#define UDESCSUB_VC_EXTENSION_UNIT			0x06

/* Table A-6: Video Class-Specific VS Interface Descriptor Subtypes */
#define	UDESCSUB_VS_UNDEFINED				0x00
#define UDESCSUB_VS_INPUT_HEADER			0x01
#define UDESCSUB_VS_OUTPUT_HEADER			0x02
#define UDESCSUB_VS_STILL_IMAGE_FRAME			0x03
#define UDESCSUB_VS_FORMAT_UNCOMPRESSED			0x04
#define UDESCSUB_VS_FRAME_UNCOMPRESSED			0x05
#define UDESCSUB_VS_FORMAT_MJPEG			0x06
#define UDESCSUB_VS_FRAME_MJPEG				0x07
#define UDESCSUB_VS_FORMAT_MPEG2TS			0x0a
#define UDESCSUB_VS_FORMAT_DV				0x0c
#define UDESCSUB_VS_COLORFORMAT				0x0d
#define UDESCSUB_VS_FORMAT_FRAME_BASED			0x10
#define UDESCSUB_VS_FRAME_FRAME_BASED			0x11
#define UDESCSUB_VS_FORMAT_STREAM_BASED			0x12

/* Table A-8: Video Class-Specific Request Codes */
#define RC_UNDEFINED					0x00
#define SET_CUR						0x01
#define GET_CUR						0x81
#define GET_MIN						0x82
#define GET_MAX						0x83
#define GET_RES						0x84
#define GET_LEN						0x85
#define GET_INFO					0x86
#define GET_DEF						0x87

/* Table A-9: Video Control Interface Control Selectors */
#define VC_CONTROL_UNDEFINED				0x00
#define VC_VIDEO_POWER_MODE_CONTROL			0x01
#define VC_REQUEST_ERROR_CODE_CONTROL			0x02

/* Table A-11: Selector Unit Control Selectors */
#define	SU_CONTROL_UNDEFINED				0x00
#define	SU_INPUT_SELECT_CONTROL 			0x01

/* Table A-12: Camera Terminal Control Selectors */
#define	CT_CONTROL_UNDEFINED				0x00
#define	CT_SCANNING_MODE_CONTROL			0x01
#define	CT_AE_MODE_CONTROL				0x02
#define	CT_AE_PRIORITY_CONTROL				0x03
#define	CT_EXPOSURE_TIME_ABSOLUTE_CONTROL		0x04
#define	CT_EXPOSURE_TIME_RELATIVE_CONTROL		0x05
#define	CT_FOCUS_ABSOLUTE_CONTROL			0x06
#define	CT_FOCUS_RELATIVE_CONTROL			0x07
#define	CT_FOCUS_AUTO_CONTROL				0x08
#define	CT_IRIS_ABSOLUTE_CONTROL			0x09
#define	CT_IRIS_RELATIVE_CONTROL			0x0a
#define	CT_ZOOM_ABSOLUTE_CONTROL			0x0b
#define	CT_ZOOM_RELATIVE_CONTROL			0x0c
#define	CT_PANTILT_ABSOLUTE_CONTROL			0x0d
#define	CT_PANTILT_RELATIVE_CONTROL			0x0e
#define	CT_ROLL_ABSOLUTE_CONTROL			0x0f
#define	CT_ROLL_RELATIVE_CONTROL			0x10
#define	CT_PRIVACY_CONTROL				0x11

/* Table A-13: Processing Unit Control Selectors */
#define	PU_CONTROL_UNDEFINED				0x00
#define	PU_BACKLIGHT_COMPENSATION_CONTROL		0x01
#define	PU_BRIGHTNESS_CONTROL				0x02
#define	PU_CONTRAST_CONTROL				0x03
#define	PU_GAIN_CONTROL 				0x04
#define	PU_POWER_LINE_FREQUENCY_CONTROL 		0x05
#define	PU_HUE_CONTROL					0x06
#define	PU_SATURATION_CONTROL				0x07
#define	PU_SHARPNESS_CONTROL				0x08
#define	PU_GAMMA_CONTROL				0x09
#define	PU_WHITE_BALANCE_TEMPERATURE_CONTROL		0x0a
#define	PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL	0x0b
#define	PU_WHITE_BALANCE_COMPONENT_CONTROL		0x0c
#define	PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL 	0x0d
#define	PU_DIGITAL_MULTIPLIER_CONTROL			0x0e
#define	PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL		0x0f
#define	PU_HUE_AUTO_CONTROL				0x10
#define	PU_ANALOG_VIDEO_STANDARD_CONTROL		0x11
#define	PU_ANALOG_LOCK_STATUS_CONTROL			0x12

/* Table A-15: VideoStreaming Interface Control Selectors */
#define	VS_CONTROL_UNDEFINED				0x00
#define	VS_PROBE_CONTROL				0x01
#define	VS_COMMIT_CONTROL				0x02
#define	VS_STILL_PROBE_CONTROL				0x03
#define	VS_STILL_COMMIT_CONTROL 			0x04
#define	VS_STILL_IMAGE_TRIGGER_CONTROL			0x05
#define	VS_STREAM_ERROR_CODE_CONTROL			0x06
#define	VS_GENERATE_KEY_FRAME_CONTROL			0x07
#define	VS_UPDATE_FRAME_SEGMENT_CONTROL			0x08
#define	VS_SYNC_DELAY_CONTROL				0x09

/* probe commit bmRequests */
#define	UVIDEO_SET_IF					0x21
#define	UVIDEO_GET_IF					0xa1
#define	UVIDEO_SET_EP					0x22
#define	UVIDEO_GET_EP					0xa2

/* Table B-1: USB Terminal Types */
#define	TT_VENDOR_SPECIFIC				0x0100
#define	TT_STREAMING					0x0101

/* Table B-2: Input Terminal Types */
#define	ITT_VENDOR_SPECIFIC				0x0200
#define	ITT_CAMERA					0x0201
#define	ITT_MEDIA_TRANSPORT_INPUT			0x0202

/* Table B-3: Output Terminal Types */
#define	OTT_VENDOR_SPECIFIC				0x0300
#define	OTT_DISPLAY					0x0301
#define	OTT_MEDIA_TRANSPORT_OUTPUT			0x0302

/* Table B-4: External Terminal Types */
#define	EXTERNAL_VENDOR_SPECIFIC			0x0400
#define	COMPOSITE_CONNECTOR				0x0401
#define	SVIDEO_CONNECTOR				0x0402
#define	COMPONENT_CONNECTOR				0x0403

/* Table 3-3: VC Interface Header Descriptor */
struct usb_video_header_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uWord	bcdUVC;
	uWord	wTotalLength;
	uDWord	dwClockFrequency;
	uByte	bInCollection;
} __packed;

struct usb_video_header_desc_all {
	struct usb_video_header_desc	*fix;
	uByte				*baInterfaceNr;
};

/* Table 3-4: Input Terminal Descriptor */
struct usb_video_input_terminal_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bTerminalID;
	uWord	wTerminalType;
	uByte	bAssocTerminal;
	uByte	iTerminal;
} __packed;

/* Table 3-5: Output Terminal Descriptor */
struct usb_video_output_terminal_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bTerminalID;
	uWord	wTerminalType;
	uByte	bAssocTerminal;
	uByte	bSourceID;
	uByte	iTerminal;
} __packed;

/* Table 3-6: Camera Terminal Descriptor */
struct usb_video_camera_terminal_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bTerminalID;
	uWord	wTerminalType;
	uByte	bAssocTerminal;
	uByte	iTerminal;
	uWord	wObjectiveFocalLengthMin;
	uWord	wObjectiveFocalLengthMax;
	uWord	wOcularFocalLength;
	uByte	bControlSize;
	uByte	*bmControls;
} __packed;

/* Table 3-8: VC Processing Unit Descriptor */
struct usb_video_vc_processing_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bUnitID;
	uByte	bSourceID;
	uWord	wMaxMultiplier;
	uByte	bControlSize;
	uByte	bmControls[255]; /* [bControlSize] */
	/* uByte iProcessing; */
	/* uByte bmVideoStandards; */
} __packed;

/* Table 3-9: VC Extension Unit Descriptor */
struct usb_video_vc_extension_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bUnitID;
	uByte	guidExtensionCode[16];
	uByte	bNumControls;
	uByte	bNrInPins;
} __packed;

/* Table 3-11: VC Endpoint Descriptor */
struct usb_video_vc_endpoint_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uWord	wMaxTransferSize;
} __packed;

/* Table 3-13: Interface Input Header Descriptor */
struct usb_video_input_header_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bNumFormats;
	uWord	wTotalLength;
	uByte	bEndpointAddress;
	uByte	bmInfo;
	uByte	bTerminalLink;
	uByte	bStillCaptureMethod;
	uByte	bTriggerSupport;
	uByte	bTriggerUsage;
	uByte	bControlSize;
} __packed;

struct usb_video_input_header_desc_all {
	struct usb_video_input_header_desc	*fix;
	uByte					*bmaControls;
};

/* Table 3-18: Color Matching Descriptor */
struct usb_video_color_matching_descr {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bColorPrimaries;
	uByte	bTransferCharacteristics;
	uByte	bMatrixCoefficients;
} __packed; 

/* Table 4-47: Video Probe and Commit Controls */
struct usb_video_probe_commit {
	uWord	bmHint;
	uByte	bFormatIndex;
	uByte	bFrameIndex;
	uDWord	dwFrameInterval;
	uWord	wKeyFrameRate;
	uWord	wPFrameRate;
	uWord	wCompQuality;
	uWord	wCompWindowSize;
	uWord	wDelay;
	uDWord	dwMaxVideoFrameSize;
	uDWord	dwMaxPayloadTransferSize;
	uDWord	dwClockFrequency;
	uByte	bmFramingInfo;
	uByte	bPreferedVersion;
	uByte	bMinVersion;
	uByte	bMaxVersion;
} __packed;

/*
 * USB Video Payload Uncompressed
 */
/* Table 2-1: Compression Formats */
#define	UVIDEO_FORMAT_GUID_YUY2	{			\
    0x59, 0x55, 0x59, 0x32, 0x00, 0x00, 0x10, 0x00,	\
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }

#define	UVIDEO_FORMAT_GUID_NV12	{			\
    0x4e, 0x56, 0x31, 0x32, 0x00, 0x00, 0x10, 0x00,	\
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38,	0x9b, 0x71 }

#define	UVIDEO_FORMAT_GUID_UYVY	{			\
    0x55, 0x59, 0x56, 0x59, 0x00, 0x00, 0x10, 0x00,	\
    0x80, 0x00, 0x00, 0xaa, 0x00, 0x38,	0x9b, 0x71 }

/*
 * USB Video Payload MJPEG
 */
/* Table 2-1: Stream Header Format for the Motion-JPEG */
#define UVIDEO_SH_MAX_LEN	12
#define UVIDEO_SH_MIN_LEN	2
struct usb_video_stream_header {
	uByte	bLength;
	uByte	bFlags;
#define	UVIDEO_SH_FLAG_FID	(1 << 0)
#define	UVIDEO_SH_FLAG_EOF	(1 << 1)
#define	UVIDEO_SH_FLAG_PTS	(1 << 2)
#define	UVIDEO_SH_FLAG_SCR	(1 << 3)
#define	UVIDEO_SH_FLAG_RES	(1 << 4)
#define	UVIDEO_SH_FLAG_STI	(1 << 5)
#define	UVIDEO_SH_FLAG_ERR	(1 << 6)
#define	UVIDEO_SH_FLAG_EOH	(1 << 7)
	/* TODO complete struct */
} __packed;

/* Table 3-1: Motion-JPEG Video Format Descriptor */
struct usb_video_format_mjpeg_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bFormatIndex;
	uByte	bNumFrameDescriptors;
	uByte	bmFlags;
	uByte	bDefaultFrameIndex;
	uByte	bAspectRatioX;
	uByte	bAspectRatioY;
	uByte	bmInterlaceFlags;
	uByte	bCopyProtect;
} __packed;

/* Table 3-2: Video Frame Descriptor (same for mjpeg and uncompressed)*/
struct usb_video_frame_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bFrameIndex;
	uByte	bmCapabilities;
	uWord	wWidth;
	uWord	wHeight;
	uDWord	dwMinBitRate;
	uDWord	dwMaxBitRate;
	uDWord	dwMaxVideoFrameBufferSize;
	uDWord	dwDefaultFrameInterval;
	uByte	bFrameIntervalType;
	/* TODO add continous/discrete frame intervals (Table 3-3/3-4) */
} __packed;

/*
 * USB Video Payload Uncompressed
 */
/* Table 3-1: Uncompressed Video Format Descriptor */
struct usb_video_format_uncompressed_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bFormatIndex;
	uByte	bNumFrameDescriptors;
	uByte	guidFormat[16];
	uByte	bBitsPerPixel;
	uByte	bDefaultFrameIndex;
	uByte	bAspectRatioX;
	uByte	bAspectRatioY;
	uByte	bmInterlaceFlags;
	uByte	bCopyProtect;
} __packed;

/*
 * Driver specific private definitions.
 */
struct uvideo_format_desc {
	uByte	bLength;
	uByte	bDescriptorType;
	uByte	bDescriptorSubtype;
	uByte	bFormatIndex;
	uByte	bNumFrameDescriptors;
	union {
		/* mjpeg */
		struct {
			uByte	bmFlags;
			uByte	bDefaultFrameIndex;
			uByte	bAspectRatioX;
			uByte	bAspectRatioY;
			uByte	bmInterlaceFlags;
			uByte	bCopyProtect;
		} mjpeg;

		/* uncompressed */
		struct {
			uByte	guidFormat[16];
			uByte	bBitsPerPixel;
			uByte	bDefaultFrameIndex;
			uByte	bAspectRatioX;
			uByte	bAspectRatioY;
			uByte	bmInterlaceFlags;
			uByte	bCopyProtect;
		} uc;
	} u;
} __packed;

#define UVIDEO_NFRAMES_MAX	40
struct uvideo_isoc_xfer {
	struct uvideo_softc	*sc;
	usbd_xfer_handle	 xfer;
	void			*buf;
	uint16_t		 size[UVIDEO_NFRAMES_MAX];
};

struct uvideo_bulk_xfer {
	struct uvideo_softc	*sc;
	usbd_xfer_handle	 xfer;
	void			*buf;
	uint16_t		 size;
};

#define UVIDEO_IXFERS		3
struct uvideo_vs_iface {
	usbd_interface_handle  	 ifaceh;
	usbd_pipe_handle	 pipeh;
	int			 iface;
	int			 numalts;
	int			 curalt;
	int			 endpoint;
	uint32_t		 psize;
	int			 bulk_endpoint;
	int			 bulk_running;
	struct uvideo_isoc_xfer	 ixfer[UVIDEO_IXFERS];
	struct uvideo_bulk_xfer	 bxfer;
};

struct uvideo_frame_buffer {
	int		 sample;
	uint8_t		 fid;
	int		 offset;
	int		 buf_size;
	uint8_t		*buf;
};

#define UVIDEO_MAX_BUFFERS	32
struct uvideo_mmap {
	SIMPLEQ_ENTRY(uvideo_mmap)	q_frames;
	uint8_t				*buf;
	struct v4l2_buffer		 v4l2_buf;
};
typedef SIMPLEQ_HEAD(, uvideo_mmap) q_mmap;

struct uvideo_format_group {
	uint32_t				 pixelformat;
	uint8_t					 format_dfidx;
	struct uvideo_format_desc		*format;
	/* frame descriptors for mjpeg and uncompressed are identical */
#define UVIDEO_MAX_FRAME			 32
	struct usb_video_frame_desc		*frame_cur;
	struct usb_video_frame_desc		*frame[UVIDEO_MAX_FRAME];
	int					 frame_num;
};

struct uvideo_res {
	int width;
	int height;
	int fidx;
};

struct uvideo_controls {
	int		cid;
	int		type;
	char		name[32];
	uint8_t         ctrl_bit;
	uint16_t	ctrl_selector;
	uint16_t	ctrl_len;
	int		sig;
} uvideo_ctrls[] = {
        /*
         * Processing Unit Controls
         */
	{
	    V4L2_CID_BRIGHTNESS,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Brightness",
	    0,
	    PU_BRIGHTNESS_CONTROL,
	    2,
	    1
	},
	{
	    V4L2_CID_CONTRAST,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Contrast",
	    1,
	    PU_CONTRAST_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_HUE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Hue",
	    2,
	    PU_HUE_CONTROL,
	    2,
	    1
	},
	{
	    V4L2_CID_SATURATION,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Saturation",
	    3,
	    PU_SATURATION_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_SHARPNESS,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Sharpness",
	    4,
	    PU_SHARPNESS_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_GAMMA,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Gamma",
	    5,
	    PU_GAMMA_CONTROL,
	    2,
	    0
	},
	{
	    V4L2_CID_WHITE_BALANCE_TEMPERATURE,
	    V4L2_CTRL_TYPE_INTEGER,
	    "White Balance Temperature",
	    6,
	    PU_WHITE_BALANCE_TEMPERATURE_CONTROL,
	    2,
	    0
	},
#if 0
        /* XXX Two V4L2 ids mapping one UVC control */
	{
	    V4L2_CID_RED_BALANCE, /* V4L2_CID_BLUE_BALANCE */
	    V4L2_CTRL_TYPE_INTEGER,
	    "White Balance Red Component", /* Blue Component */
	    7,
	    PU_WHITE_BALANCE_COMPONENT_CONTROL,
	    4,
	    0
	},
#endif
        {
            V4L2_CID_BACKLIGHT_COMPENSATION,
            V4L2_CTRL_TYPE_INTEGER,
            "Backlight Compensation",
            8,
            PU_BACKLIGHT_COMPENSATION_CONTROL,
            2,
	    0
        },
	{
	    V4L2_CID_GAIN,
	    V4L2_CTRL_TYPE_INTEGER,
	    "Gain",
	    9,
	    PU_GAIN_CONTROL,
	    2,
	    0
	},
        {
            V4L2_CID_POWER_LINE_FREQUENCY,
            V4L2_CTRL_TYPE_MENU,
            "Power Line Frequency",
            10,
            PU_POWER_LINE_FREQUENCY_CONTROL,
            2,
	    0
        },
        {
            V4L2_CID_HUE_AUTO,
            V4L2_CTRL_TYPE_BOOLEAN,
            "Hue Auto",
            11,
            PU_HUE_AUTO_CONTROL,
            1,
	    0
        },
        {
            V4L2_CID_AUTO_WHITE_BALANCE,
            V4L2_CTRL_TYPE_BOOLEAN,
            "White Balance Temperature Auto",
            12,
            PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL,
            1,
	    0
        },
        {
            V4L2_CID_AUTO_WHITE_BALANCE,
            V4L2_CTRL_TYPE_BOOLEAN,
            "White Balance Component Auto",
            13,
            PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL,
            1,
	    0
        },
#if 0
        /* XXX No V4L2 CID for these controls? */
        {
            V4L2_CID_XXX,
            V4L2_CTRL_TYPE_INTEGER,
            "Digital Multiplier",
            14,
            PU_DIGITAL_MULTIPLIER_CONTROL,
            2,
	    0
        },
        {
            V4L2_CID_XXX,
            V4L2_CTRL_TYPE_INTEGER,
            "Digital Multiplier Limit",
            15,
            PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL,
            2,
	    0
        },
        {
            V4L2_CID_XXX,
            V4L2_CTRL_TYPE_INTEGER,
            "Analog Video Standard",
            16,
            PU_ANALOG_VIDEO_STANDARD_CONTROL,
            1,
	    0
        },
        {
            V4L2_CID_XXX,
            V4L2_CTRL_TYPE_INTEGER,
            "Analog Lock Status",
            17,
            PU_ANALOG_LOCK_STATUS_CONTROL,
            1,
	    0
        },
#endif
	{ 0, 0, "", 0, 0, 0, 0 }
};

struct uvideo_softc {
	struct device				 sc_dev;
	usbd_device_handle			 sc_udev;
	int					 sc_nifaces;
	usbd_interface_handle			*sc_ifaces;

	struct device				*sc_videodev;

	int					 sc_enabled;
	int					 sc_dying;
	int					 sc_max_fbuf_size;
	int					 sc_negotiated_flag;

	struct uvideo_frame_buffer		 sc_frame_buffer;

	struct uvideo_mmap			 sc_mmap[UVIDEO_MAX_BUFFERS];
	uint8_t					*sc_mmap_buffer;
	q_mmap					 sc_mmap_q;
	int					 sc_mmap_count;
	int					 sc_mmap_cur;
	int					 sc_mmap_flag;

	struct vnode				*sc_vp;
	struct usb_task				 sc_task_write;

	int					 sc_nframes;
	struct usb_video_probe_commit		 sc_desc_probe;
	struct usb_video_header_desc_all	 sc_desc_vc_header;
	struct usb_video_input_header_desc_all	 sc_desc_vs_input_header;

#define UVIDEO_MAX_PU				 8
	int					 sc_desc_vc_pu_num;
	struct usb_video_vc_processing_desc	*sc_desc_vc_pu_cur;
	struct usb_video_vc_processing_desc	*sc_desc_vc_pu[UVIDEO_MAX_PU];

#define UVIDEO_MAX_FORMAT			 8
	int					 sc_fmtgrp_idx;
	int					 sc_fmtgrp_num;
	struct uvideo_format_group		*sc_fmtgrp_cur;
	struct uvideo_format_group		 sc_fmtgrp[UVIDEO_MAX_FORMAT];

#define	UVIDEO_MAX_VS_NUM			 8
	struct uvideo_vs_iface			*sc_vs_cur;
	struct uvideo_vs_iface			 sc_vs_coll[UVIDEO_MAX_VS_NUM];

	void					*sc_uplayer_arg;
	int					*sc_uplayer_fsize;
	uint8_t					*sc_uplayer_fbuffer;
	void					 (*sc_uplayer_intr)(void *);

	struct uvideo_devs			*sc_quirk;
	usbd_status				(*sc_decode_stream_header)
						    (struct uvideo_softc *,
						    uint8_t *, int);
};

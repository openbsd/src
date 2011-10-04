/* $OpenBSD: pmsreg.h,v 1.5 2011/10/04 06:30:40 mpi Exp $ */
/* $NetBSD: psmreg.h,v 1.1 1998/03/22 15:41:28 drochner Exp $ */

#ifndef SYS_DEV_PCKBC_PMSREG_H
#define SYS_DEV_PCKBC_PMSREG_H

/* mouse commands */
#define PMS_SET_SCALE11		0xe6	/* set scaling 1:1 */
#define PMS_SET_SCALE21		0xe7	/* set scaling 2:1 */
#define PMS_SET_RES		0xe8	/* set resolution (0..3) */
#define PMS_SEND_DEV_STATUS	0xe9	/* status request */
#define PMS_SET_STREAM_MODE	0xea
#define PMS_SEND_DEV_DATA	0xeb	/* read data */
#define PMS_RESET_WRAP_MODE	0xec
#define PMS_SET_WRAP_MODE	0xed
#define PMS_SET_REMOTE_MODE	0xf0
#define PMS_SEND_DEV_ID		0xf2	/* read device type */
#define PMS_SET_SAMPLE		0xf3	/* set sampling rate */
#define PMS_DEV_ENABLE		0xf4	/* mouse on */
#define PMS_DEV_DISABLE		0xf5	/* mouse off */
#define PMS_SET_DEFAULTS	0xf6
#define PMS_RESEND		0xfe
#define PMS_RESET		0xff	/* reset */

#define PMS_RSTDONE		0xaa

/* PS/2 mouse data packet */
#define PMS_PS2_BUTTONSMASK	0x07
#define PMS_PS2_BUTTON1		0x01	/* left */
#define PMS_PS2_BUTTON2		0x04	/* middle */
#define PMS_PS2_BUTTON3		0x02	/* right */
#define PMS_PS2_XNEG		0x10
#define PMS_PS2_YNEG		0x20

#define PMS_INTELLI_MAGIC1	200
#define PMS_INTELLI_MAGIC2	100
#define PMS_INTELLI_MAGIC3	80
#define PMS_INTELLI_ID		0x03

#define PMS_ALPS_MAGIC1		0
#define PMS_ALPS_MAGIC2		0
#define PMS_ALPS_MAGIC3_1	10
#define PMS_ALPS_MAGIC3_2	100

/* Synaptics queries */
#define SYNAPTICS_QUE_IDENTIFY			0x00
#define SYNAPTICS_QUE_MODES			0x01
#define SYNAPTICS_QUE_CAPABILITIES		0x02
#define SYNAPTICS_QUE_MODEL			0x03
#define SYNAPTICS_QUE_SERIAL_NUMBER_PREFIX	0x06
#define SYNAPTICS_QUE_SERIAL_NUMBER_SUFFIX	0x07
#define SYNAPTICS_QUE_RESOLUTION		0x08
#define SYNAPTICS_QUE_EXT_MODEL			0x09
#define SYNAPTICS_QUE_EXT_CAPABILITIES		0x0c
#define SYNAPTICS_QUE_EXT_DIMENSIONS		0x0d

#define SYNAPTICS_CMD_SET_MODE			0x14
#define SYNAPTICS_CMD_SEND_CLIENT		0x28
#define SYNAPTICS_CMD_SET_ADV_GESTURE_MODE	0xc8

/* Identify */
#define SYNAPTICS_ID_MODEL(id)			(((id) >>  4) & 0x0f)
#define SYNAPTICS_ID_MINOR(id)			(((id) >> 16) & 0xff)
#define SYNAPTICS_ID_MAJOR(id)			((id) & 0x0f)
#define SYNAPTICS_ID_MAGIC			0x47

/* Modes bits */
#define SYNAPTICS_ABSOLUTE_MODE			(1 << 7)
#define SYNAPTICS_HIGH_RATE			(1 << 6)
#define SYNAPTICS_SLEEP_MODE			(1 << 3)
#define SYNAPTICS_DISABLE_GESTURE		(1 << 2)
#define SYNAPTICS_FOUR_BYTE_CLIENT		(1 << 1)
#define SYNAPTICS_W_MODE			(1 << 0)

/* Capability bits */
#define SYNAPTICS_CAP_EXTENDED			(1 << 23)
#define SYNAPTICS_CAP_EXTENDED_QUERIES(c)	(((c) >> 20) & 0x07)
#define SYNAPTICS_CAP_MIDDLE_BUTTON		(1 << 18)
#define SYNAPTICS_CAP_PASSTHROUGH		(1 << 7)
#define SYNAPTICS_CAP_SLEEP			(1 << 4)
#define SYNAPTICS_CAP_FOUR_BUTTON		(1 << 3)
#define SYNAPTICS_CAP_BALLISTICS		(1 << 2)
#define SYNAPTICS_CAP_MULTIFINGER		(1 << 1)
#define SYNAPTICS_CAP_PALMDETECT		(1 << 0)

/* Model ID bits */
#define SYNAPTICS_MODEL_ROT180			(1 << 23)
#define SYNAPTICS_MODEL_PORTRAIT		(1 << 22)
#define SYNAPTICS_MODEL_SENSOR(m)		(((m) >> 16) & 0x3f)
#define SYNAPTICS_MODEL_HARDWARE(m)		(((m) >> 9) & 0x7f)
#define SYNAPTICS_MODEL_NEWABS			(1 << 7)
#define SYNAPTICS_MODEL_PEN			(1 << 6)
#define SYNAPTICS_MODEL_SIMPLC			(1 << 5)
#define SYNAPTICS_MODEL_GEOMETRY(m)		((m) & 0x0f)

#define ALPS_GLIDEPOINT				(1 << 1)
#define ALPS_DUALPOINT				(1 << 2)
#define ALPS_PASSTHROUGH			(1 << 3)

/* Resolutions */
#define SYNAPTICS_RESOLUTION_X(r)		(((r) >> 16) & 0xff)
#define SYNAPTICS_RESOLUTION_Y(r)		((r) & 0xff)

/* Extended Model ID bits */
#define SYNAPTICS_EXT_MODEL_LIGHTCONTROL	(1 << 22)
#define SYNAPTICS_EXT_MODEL_PEAKDETECT		(1 << 21)
#define SYNAPTICS_EXT_MODEL_VWHEEL		(1 << 19)
#define SYNAPTICS_EXT_MODEL_EW_MODE		(1 << 18)
#define SYNAPTICS_EXT_MODEL_HSCROLL		(1 << 17)
#define SYNAPTICS_EXT_MODEL_VSCROLL		(1 << 16)
#define SYNAPTICS_EXT_MODEL_BUTTONS(em)		((em >> 12) & 0x0f)
#define SYNAPTICS_EXT_MODEL_SENSOR(em)		((em >> 10) & 0x03)
#define SYNAPTICS_EXT_MODEL_PRODUCT(em)		((em) & 0xff)

/* Extended Capability bits */
#define SYNAPTICS_EXT_CAP_CLICKPAD		(1 << 20)
#define SYNAPTICS_EXT_CAP_ADV_GESTURE		(1 << 19)
#define SYNAPTICS_EXT_CAP_MAX_DIMENSIONS	(1 << 17)
#define SYNAPTICS_EXT_CAP_CLICKPAD_2BTN		(1 << 8)

/* Extended Dimensions */
#define SYNAPTICS_DIM_X(d)			((((d) & 0xff0000) >> 11) | \
						 (((d) & 0xf00) >> 7))
#define SYNAPTICS_DIM_Y(d)			((((d) & 0xff) << 5) | \
						 (((d) & 0xf000) >> 11))

/* Typical bezel limit */
#define SYNAPTICS_XMIN_BEZEL			1472
#define SYNAPTICS_XMAX_BEZEL			5472
#define SYNAPTICS_YMIN_BEZEL			1408
#define SYNAPTICS_YMAX_BEZEL			4448

#define ALPS_XMIN_BEZEL				130
#define ALPS_XMAX_BEZEL				840
#define ALPS_YMIN_BEZEL				130
#define ALPS_YMAX_BEZEL				640

#endif /* SYS_DEV_PCKBC_PMSREG_H */

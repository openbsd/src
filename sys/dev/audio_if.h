/*	$OpenBSD: audio_if.h,v 1.30 2015/06/25 06:43:46 ratchov Exp $	*/
/*	$NetBSD: audio_if.h,v 1.24 1998/01/10 14:07:25 tv Exp $	*/

/*
 * Copyright (c) 1994 Havard Eidnes.
 * All rights reserved.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _SYS_DEV_AUDIO_IF_H_
#define _SYS_DEV_AUDIO_IF_H_

#include <sys/mutex.h>

#define AUDIO_BPS(bits)		(bits) <= 8 ? 1 : ((bits) <= 16 ? 2 : 4)

/*
 * Generic interface to hardware driver.
 */

struct audio_device;
struct audio_encoding;
struct mixer_devinfo;
struct mixer_ctrl;

struct audio_params {
	u_long	sample_rate;			/* sample rate */
	u_int	encoding;			/* mu-law, linear, etc */
	u_int	precision;			/* bits/sample */
	u_int	bps;				/* bytes/sample */
	u_int	msb;				/* data alignment */
	u_int	channels;			/* mono(1), stereo(2) */
};

struct audio_hw_if {
	int	(*open)(void *, int);	/* open hardware */
	void	(*close)(void *);		/* close hardware */
	int	(*drain)(void *);		/* Optional: drain buffers */

	/* Encoding. */
	/* XXX should we have separate in/out? */
	int	(*query_encoding)(void *, struct audio_encoding *);

	/* Set the audio encoding parameters (record and play).
	 * Return 0 on success, or an error code if the
	 * requested parameters are impossible.
	 * The values in the params struct may be changed (e.g. rounding
	 * to the nearest sample rate.)
	 */
	int	(*set_params)(void *, int, int, struct audio_params *,
		    struct audio_params *);

	/* Hardware may have some say in the blocksize to choose */
	int	(*round_blocksize)(void *, int);

	/*
	 * Changing settings may require taking device out of "data mode",
	 * which can be quite expensive.  Also, audiosetinfo() may
	 * change several settings in quick succession.  To avoid
	 * having to take the device in/out of "data mode", we provide
	 * this function which indicates completion of settings
	 * adjustment.
	 */
	int	(*commit_settings)(void *);

	/* Start input/output routines. These usually control DMA. */
	int	(*init_output)(void *, void *, int);
	int	(*init_input)(void *, void *, int);
	int	(*start_output)(void *, void *, int, void (*)(void *), void *);
	int	(*start_input)(void *, void *, int, void (*)(void *), void *);
	int	(*halt_output)(void *);
	int	(*halt_input)(void *);

	int	(*speaker_ctl)(void *, int);
#define SPKR_ON		1
#define SPKR_OFF	0

	int	(*getdev)(void *, struct audio_device *);
	int	(*setfd)(void *, int);

	/* Mixer (in/out ports) */
	int	(*set_port)(void *, struct mixer_ctrl *);
	int	(*get_port)(void *, struct mixer_ctrl *);

	int	(*query_devinfo)(void *, struct mixer_devinfo *);

	/* Allocate/free memory for the ring buffer. Usually malloc/free. */
	/* The _old interfaces have been deprecated and will not be
	   called in newer kernels if the new interfaces are present */
	void	*(*allocm)(void *, int, size_t, int, int);
	void	(*freem)(void *, void *, int);
	size_t	(*round_buffersize)(void *, int, size_t);
	paddr_t	(*mappage)(void *, void *, off_t, int);

	int	(*get_props)(void *); /* device properties */

	int	(*trigger_output)(void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *);
	int	(*trigger_input)(void *, void *, void *, int,
		    void (*)(void *), void *, struct audio_params *);
	void	(*get_default_params)(void *, int, struct audio_params *);
};

struct audio_attach_args {
	int	type;
	void	*hwif;		/* either audio_hw_if * or midi_hw_if * */
	void	*hdl;
};
#define	AUDIODEV_TYPE_AUDIO	0
#define	AUDIODEV_TYPE_MIDI	1
#define AUDIODEV_TYPE_OPL	2
#define AUDIODEV_TYPE_MPU	3
#define AUDIODEV_TYPE_RADIO	4

/* Attach the MI driver(s) to the MD driver. */
struct device *audio_attach_mi(struct audio_hw_if *, void *, struct device *);
int	       audioprint(void *, const char *);

extern struct mutex audio_lock;

#endif /* _SYS_DEV_AUDIO_IF_H_ */

/*	$OpenBSD: scsi_cd.h,v 1.5 1999/07/20 06:21:59 csapuntz Exp $	*/
/*	$NetBSD: scsi_cd.h,v 1.6 1996/03/19 03:06:39 mycroft Exp $	*/

/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with 
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */
#ifndef	_SCSI_SCSI_CD_H
#define _SCSI_SCSI_CD_H 1

struct scsi_cd_write_params_page {
        u_int8_t page_code;
        u_int8_t page_len;
        u_int8_t write_type;
#define WRITE_TYPE_DUMMY        0x10    /* do not actually write blocks */
#define WRITE_TYPE_MASK         0x0f    /* session write type */
        u_int8_t track_mode;
#define TRACK_MODE_MULTI_SESS   0xc0    /* multisession write type */
#define TRACK_MODE_FP           0x20    /* fixed packet (if in packet mode) */
#define TRACK_MODE_COPY         0x10    /* 1st higher gen of copy prot track */
#define TRACK_MODE_PREEPMPASIS  0x01    /* audio w/ preemphasis (audio) */
#define TRACK_MODE_INCREMENTAL  0x01    /* incremental data track (data) */
#define TRACK_MODE_ALLOW_COPY   0x02    /* digital copy is permitted */
#define TRACK_MODE_DATA         0x04    /* this is a data track */
#define TRACK_MODE_4CHAN        0x08    /* four channel audio */
        u_int8_t dbtype;
#define DBTYPE_MASK             0x0f    /* data block type */
        u_int8_t reserved1[2];
        u_int8_t host_appl_code;
#define HOST_APPL_CODE_MASK     0x3f    /* host application code of disk */
        u_int8_t session_format;
        u_int8_t reserved2;
        u_int8_t packet_size[4];
        u_int8_t audio_pause_len[2];
        u_int8_t media_cat_number[16];
        u_int8_t isrc[14];
        u_int8_t sub_header[4];
        u_int8_t vendir_unique[4];
};

#ifdef CDDA
/*
 * There are 2352 bytes in a CD digital audio frame.  One frame is 1/75 of a
 * second, at 44.1kHz sample rate, 16 bits/sample, 2 channels.
 *
 * The frame data have the two channels interleaved, with the left
 * channel first.  Samples are little endian 16-bit signed values.
 */
#define CD_DA_BLKSIZ		2352	/* # bytes in CD-DA frame */
#ifndef CD_NORMAL_DENSITY_CODE
#define CD_NORMAL_DENSITY_CODE	0x00	/* from Toshiba CD-ROM specs */
#endif
#ifndef CD_DA_DENSITY_CODE
#define CD_DA_DENSITY_CODE	0x82	/* from Toshiba CD-ROM specs */
#endif
#endif /* CDDA */

union scsi_cd_pages {
        struct scsi_cd_write_params_page write_params;
        struct cd_audio_page audio;
};

struct scsi_cd_mode_data {
	struct scsi_mode_header header;
	struct scsi_blk_desc blk_desc;
	union scsi_cd_pages page;
};

#define AUDIOPAGESIZE \
        (sizeof(struct scsi_mode_header) + sizeof(struct scsi_blk_desc) \
            + sizeof(struct cd_audio_page))

#endif /*_SCSI_SCSI_CD_H*/


/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 *
 *   Project: ANGEL
 *
 *     Title: Private header for channels implementations
 */

#ifndef angel_chanpriv_h
#define angel_chanpriv_h

/*
 * This describes the internal structure and flags for a channels packet.
 */

/* byte positions within channel packet */
#define CF_CHANNEL_BYTE_POS  0
#define CF_HOME_SEQ_BYTE_POS 1
#define CF_OPPO_SEQ_BYTE_POS 2
#define CF_FLAGS_BYTE_POS    3
#define CF_DATA_BYTE_POS     4

/* flags for FLAGS field */
#define CF_RELIABLE  (1 << 0)    /* use reliable channels protocol */
#define CF_RESEND    (1 << 1)    /* this is a renegotiation packet */
#define CF_HEARTBEAT (1 << 2)    /* heartbeat packet - prod target into sync */

/* byte positions within buffer */
#define CB_LINK_BYTE_POS        0   /* the link pointer */
#define CB_CHAN_HEADER_BYTE_POS 4   /* the channel frame starts here */

/* macro to get buffer position of packet component */
#define CB_PACKET(x) (CB_CHAN_HEADER_BYTE_POS + (x))

/* byte offset of packet data within buffer */
#define CB_CHAN_DATA_BYTE_POS   (CB_PACKET(CF_DATA_BYTE_POS))

/* access the link in a buffer, where b is byte pointer to buffer */
#define CB_LINK(b) ((p_Buffer)(&(b)[0]))

#define invalidChannelID(chan)  (((int)(chan)) < 0 || \
                                 (chan) >= CI_NUM_CHANNELS)

#endif /* ndef angel_chanpriv_h */

/* EOF chanpriv.h */

#ifndef NCHAN_H
#define NCHAN_H


/*
	SSH Protocol 1.5 aka New Channel Protocol
	Thanks to Martina, Axel and everyone who left Erlangen, leaving me bored.
	Written by Markus Friedl in October 1999

	Protocol versions 1.3 and 1.5 differ in the handshake protocol used for the
	tear down of channels:

	1.3:	strict request-ack-protocol:
		CLOSE	->
			<-  CLOSE_CONFIRM

	1.5:	uses variations of:
		IEOF	->
			<-  OCLOSE
			<-  IEOF
		OCLOSE	->

	See the debugging output from 'ssh -v' and 'sshd -d' in ssh-1.2.27, for example.

	Details: (for Channel data structure see channels.h)

	the output_buffer gets data received from the remote peer and is written to the socket,
	the input_buffer gets data from the socket and is sent to remote peer.
	the socket represents the local object communicating with an object reachable via the peer

		PEER A					PEER B

	read(sock, input_buffer) < 0;
	shutdown_read();
	flush(input_buffer) =: DATA
	send(DATA)			->      rcvd(DATA)                                         
						write(sock, output_buffer:=DATA);                  
	send(IEOF)			->	rcvd(IEOF)                                         
						shutdown_write() if:                               
							a) write fails                             
							b) rcvd_IEOF==true && output_buffer==empty
					<-	send(OCLOSE)
	rcvd(OCLOSE)				destroy channel
	shutdown_read() if not already
	destroy channel

	Note that each side can close the channel only if 2 messages
	have been sent and received and the associated socket has been shutdown, see below:
*/


enum {
	/* ssh-proto-1.5 overloads message-types */
	CHAN_IEOF   = SSH_MSG_CHANNEL_CLOSE,			/* no more data from sender */
	CHAN_OCLOSE = SSH_MSG_CHANNEL_CLOSE_CONFIRMATION,	/* all received data has been output */

	/* channel close flags */
	CHAN_IEOF_SENT 		= 0x01,	
	CHAN_IEOF_RCVD 		= 0x02,	
	CHAN_OCLOSE_SENT 	= 0x04,
	CHAN_OCLOSE_RCVD 	= 0x08,
	CHAN_SHUT_RD 		= 0x10,
	CHAN_SHUT_WR 		= 0x20,	

	/* a channel can be removed if ALL the following flags are set: */
	CHAN_CLOSED 		= CHAN_IEOF_SENT | CHAN_IEOF_RCVD |
				  CHAN_OCLOSE_SENT | CHAN_OCLOSE_RCVD |
				  CHAN_SHUT_RD | CHAN_SHUT_WR
};

void chan_del_if_dead(Channel *c);
void chan_rcvd_ieof(Channel *c);
void chan_rcvd_oclose(Channel *c);
void chan_send_ieof(Channel *c);
void chan_send_oclose(Channel *c);
void chan_shutdown_read(Channel *c);
void chan_shutdown_write(Channel *c);
#endif

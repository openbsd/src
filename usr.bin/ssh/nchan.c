#include "includes.h"
RCSID("$Id: nchan.c,v 1.2 1999/10/16 22:29:01 markus Exp $");

#include "ssh.h"

#include "buffer.h"
#include "channels.h"
#include "packet.h"
#include "nchan.h"

void
dump_chan(Channel *c){
	debug("chan %d type %d flags 0x%x", c->self, c->type, c->flags);
}
void
chan_rcvd_ieof(Channel *c){
	dump_chan(c);
	if(c->flags & CHAN_IEOF_RCVD){
		debug("chan_rcvd_ieof twice: %d",c->self);
		return;
	}
	debug("rcvd_CHAN_IEOF %d",c->self);
	c->flags |= CHAN_IEOF_RCVD;
	/* cannot clear input buffer. remaining data has to be sent to client */
	chan_del_if_dead(c);
}
void
chan_rcvd_oclose(Channel *c){
	dump_chan(c);
	if(c->flags & CHAN_OCLOSE_RCVD){
		debug("chan_rcvd_oclose twice: %d",c->self);
		return;
	}
	debug("rcvd_CHAN_OCLOSE %d",c->self);
	c->flags |= CHAN_OCLOSE_RCVD;
	/* our peer can no longer consume, so there is not need to read */
	chan_shutdown_read(c);
	buffer_consume(&c->output, buffer_len(&c->output));
	/* Note: for type==OPEN IEOF is sent by channel_output_poll() */
	chan_del_if_dead(c);
}
void
chan_send_ieof(Channel *c){
	if(c->flags & CHAN_IEOF_SENT){
		/* this is ok: it takes some time before we get OCLOSE */
		/* debug("send_chan_ieof twice %d", c->self); */
		return;
	}
	debug("send_CHAN_IEOF %d", c->self);
	packet_start(CHAN_IEOF);
	packet_put_int(c->remote_id);
	packet_send();
	c->flags |= CHAN_IEOF_SENT;
	dump_chan(c);
}
void
chan_send_oclose(Channel *c){
	if(c->flags & CHAN_OCLOSE_SENT){
		debug("send_chan_oclose twice %d", c->self);
		return;
	}
	debug("send_CHAN_OCLOSE %d", c->self);
	packet_start(CHAN_OCLOSE);
	packet_put_int(c->remote_id);
	packet_send();
	c->flags |= CHAN_OCLOSE_SENT;
	dump_chan(c);
}
void
chan_shutdown_write(Channel *c){
	if(c->flags & CHAN_SHUT_WR){
		debug("chan_shutdown_write twice %d",c->self);
		return;
	}
	debug("chan_shutdown_write %d", c->self);
	if(shutdown(c->sock, SHUT_WR)<0)
		error("chan_shutdown_write failed %.100s", strerror(errno));
	c->flags |= CHAN_SHUT_WR;
	/* clear output buffer, since there is noone going to read the data
	   we just closed the output-socket */
	/* buffer_consume(&c->output, buffer_len(&c->output)); */
}
void
chan_shutdown_read(Channel *c){
	if(c->flags & CHAN_SHUT_RD){
		/* chan_shutdown_read is called for read-errors and OCLOSE */
		/* debug("chan_shutdown_read twice %d",c->self); */
		return;
	}
	debug("chan_shutdown_read %d", c->self);
	if(shutdown(c->sock, SHUT_RD)<0)
		error("chan_shutdown_read failed %.100s", strerror(errno));
	c->flags |= CHAN_SHUT_RD;
}
void
chan_del_if_dead(Channel *c){
	if(c->flags == CHAN_CLOSED){
		debug("channel %d closing",c->self);
		channel_free(c->self);
	}
}

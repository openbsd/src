/*
 * $Id: server.h,v 1.1.1.1 1997/11/23 20:27:36 brian Exp $
 */

extern int server;

extern int ServerLocalOpen(const char *, mode_t);
extern int ServerTcpOpen(int);
extern void ServerClose(void);

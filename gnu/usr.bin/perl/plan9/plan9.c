#include "EXTERN.h"
#include "perl.h"

/* Functions mentioned in <sys/socket.h> but not implemented */

int getsockopt(int a, int b, int c, void *d, int *e)
{
    croak("Function \"getsockopt\" not implemented in this version of perl.");
    return (int)NULL; 
}

int setsockopt(int a, int b, int c, void *d, int *e)
{
    croak("Function \"setsockopt\" not implemented in this version of perl.");
    return (int)NULL;
}


int recvmsg(int a, struct msghdr *b, int c)
{
    croak("Function \"recvmsg\" not implemented in this version of perl.");
    return (int)NULL;
} 

int sendmsg(int a, struct msghdr *b, int c)
{
    croak("Function \"sendmsg\" not implemented in this version of perl.");
    return (int)NULL;
} 


/* Functions mentioned in <netdb.h> but not implemented */
struct netent *getnetbyname(const char *a)
{
    croak("Function \"getnetbyname\" not implemented in this version of perl.");
    return (struct netent *)NULL;
}

struct netent *getnetbyaddr(long a, int b)
{
    croak("Function \"getnetbyaddr\" not implemented in this version of perl.");
    return (struct netent *)NULL;
}

struct netent *getnetent()
{
    croak("Function \"getnetent\" not implemented in this version of perl.");
    return (struct netent *)NULL;
}

struct protoent *getprotobyname(const char *a)
{
    croak("Function \"getprotobyname\" not implemented in this version of perl.");
    return (struct protoent *)NULL;
}

struct protoent *getprotobynumber(int a)
{
    croak("Function \"getprotobynumber\" not implemented in this version of perl.");
    return (struct protoent *)NULL;
}

struct protoent *getprotoent()
{
    croak("Function \"getprotoent\" not implemented in this version of perl.");
    return (struct protoent *)NULL;
}

struct servent *getservbyport(int a, const char *b)
{
    croak("Function \"getservbyport\" not implemented in this version of perl.");
    return (struct servent *)NULL;
}

struct servent *getservent()
{
    croak("Function \"getservent\" not implemented in this version of perl.");
    return (struct servent *)NULL;
}

void sethostent(int a)
{
    croak("Function \"sethostent\" not implemented in this version of perl.");
}

void setnetent(int a)
{
    croak("Function \"setnetent\" not implemented in this version of perl.");
}

void setprotoent(int a)
{
    croak("Function \"setprotoent\" not implemented in this version of perl.");
}

void setservent(int a)
{
    croak("Function \"setservent\"  not implemented in this version of perl.");
}

void endnetent()
{
    croak("Function \"endnetent\" not implemented in this version of perl.");
}

void endprotoent()
{
    croak("Function \"endprotoent\" not implemented in this version of perl.");
}

void endservent()
{
    croak("Function \"endservent\" not implemented in this version of perl.");
}

int tcdrain(int)
{
croak("Function \"tcdrain\" not implemented in this version of perl.");
}

int tcflow(int, int)
{
croak("Function \"tcflow\" not implemented in this version of perl.");
}

int tcflush(int, int)
{
croak("Function \"tcflush\" not implemented in this version of perl.");
}

int tcsendbreak(int, int)
{
croak("Function \"tcsendbreak\" not implemented in this version of perl.");
}

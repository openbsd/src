/*	$OpenBSD: slav_locl.h,v 1.3 1998/11/28 23:41:01 art Exp $	*/

#ifndef __slav_locl_h
#define __slav_locl_h

#include <kerberosIV/site.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <netdb.h>

#include <kerberosIV/krb.h>
#include <kerberosIV/krb_db.h>
#include "klog.h"
#include <kerberosIV/prot.h>
#include "kdc.h"

#endif /*  __slav_locl_h */

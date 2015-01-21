
RPCGEN = rpcgen -C

AMQ_SVC_EXTRA = amq_svc.extra

all: amq.h amq_clnt.c amq_svc.c amq_xdr.c

amq.h: amq.x
	${RPCGEN} -o $@ -h amq.x
amq_xdr.c: amq.x
	${RPCGEN} -o $@ -c -i 100 amq.x
amq_clnt.c: amq.x
	${RPCGEN} -o $@ -l amq.x
amq_svc.c: amq.x ${AMQ_SVC_EXTRA} Makefile
	${RPCGEN} -o $@.tmp -m amq.x
	printf '/"amq.h"/a\n#include "am.h"\n#include <arpa/inet.h>\n.\n/_msgout(/-1,/^}/d\n/local/r %s\n/_msgout/\n.,+1c\n\t\tplog(XLOG_FATAL, "unable to free rpc arguments in amqprog");\n\t\tgoing_down(1);\n.\ng/_rpcsvcdirty/d\nwq\n' ${AMQ_SVC_EXTRA} | \
		ed -s $@.tmp >/dev/null
	mv $@.tmp $@



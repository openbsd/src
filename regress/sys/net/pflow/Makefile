# $OpenBSD: Makefile,v 1.4 2014/03/29 11:49:41 florian Exp $
#
#  template_v10
#  ----------------
#  * create pflow0 sending from 127.0.0.1 to 127.0.0.1:9996
#  * wait for first template and compare it to known good template
#  * destroy pflow0
#
#  flow_10_{4,6}
#  -----------------
#  * create pflow0 sending from 127.0.0.1 to 127.0.0.1:9996
#  * add 10.11.12.13 and 2001:db8::13 to lo0
#  * load pf ruleset which skips on all interfaces except lo0 to not lose
#    existing ssh sessions, e.g.:
#	set skip on {em0 cas0 cas1 enc0}
#	pass on lo0 no state
#	pass on lo0 proto tcp from port 12345 to port 12346 keep state (pflow)
#  * enable pf
#  * generate IPv4 or IPv6 traffic
#  * wait for flow and compare it with known good flow
#  * destroy pflow0
#  * delete 10.11.12.13 and 2001:db8::13 from lo0
#
# ifconfig
# --------
# Take up pflow0 and test various combinations of flowdst, flowsrc and proto.
# Destroy it in the end.

REGRESS_TARGETS=template_v10 flow_10_4 flow_10_6 ifconfig

PROG=   gen_traffic
LDADD=  -levent

template_v10:
	perl -I ${.CURDIR} ${.CURDIR}/template.pl 10 \
	    | cmp -s ${.CURDIR}/template.v10 /dev/stdin

flow_10_4: gen_traffic
	perl -I ${.CURDIR} ${.CURDIR}/flow.pl 10 4 \
	    | cmp -s ${.CURDIR}/flow.10_4 /dev/stdin
flow_10_6: gen_traffic
	perl -I ${.CURDIR} ${.CURDIR}/flow.pl 10 6 \
	    | cmp -s ${.CURDIR}/flow.10_6 /dev/stdin

ifconfig:
	sh ${.CURDIR}/ifconfig.sh 2>&1 \
	    | cmp -s ${.CURDIR}/ifconfig.ok /dev/stdin

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>

#ifndef NPPPD_RADIUS_H
#define NPPPD_RADIUS_H 1

#ifdef __cplusplus
extern "C" {
#endif

void  ppp_proccess_radius_framed_ip (npppd_ppp *, RADIUS_PACKET *);
int   ppp_set_radius_attrs_for_authreq (npppd_ppp *, radius_req_setting *, RADIUS_PACKET *);
void  npppd_ppp_radius_acct_start (npppd *, npppd_ppp *);
void  npppd_ppp_radius_acct_stop (npppd *, npppd_ppp *);

#ifdef __cplusplus
}
#endif
#endif

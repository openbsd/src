#ifndef _DEV_IC_RTWPHY_H
#define _DEV_IC_RTWPHY_H

struct rtw_rf *rtw_sa2400_create(struct rtw_regs *, rtw_rf_write_t, int);
struct rtw_rf *rtw_max2820_create(struct rtw_regs *, rtw_rf_write_t, int);

int rtw_phy_init(struct rtw_regs *, struct rtw_rf *, u_int8_t, u_int8_t, u_int,
    int, int, enum rtw_pwrstate);

#endif /* _DEV_IC_RTWPHY_H */

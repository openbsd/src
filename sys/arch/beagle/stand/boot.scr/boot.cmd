setenv loadaddr 0x82800000 ;
setenv bootargs sd0i:/bsd.umg ;
fatload mmc 0 ${loadaddr} bsd.umg ;
bootm ${loadaddr}

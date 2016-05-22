run findfdt ;
load mmc ${mmcdev}:1 ${loadaddr} bsd.umg ;
load mmc ${mmcdev}:1 ${fdt_addr_r} ${fdtfile} ;
bootm ${loadaddr} - ${fdt_addr_r} ;

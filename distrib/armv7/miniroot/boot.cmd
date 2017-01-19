setenv fdtfile imx6q-sabrelite.dtb ;
load ${dtype} ${disk}:1 ${fdt_addr} ${fdtfile} ;
load ${dtype} ${disk}:1 ${loadaddr} efi/boot/bootarm.efi ;
bootefi ${loadaddr} ${fdt_addr} ;

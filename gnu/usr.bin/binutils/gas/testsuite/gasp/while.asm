	donkey
bar	.ASSIGNA	0
	.AWHILE	\&bar LT 5	
	HI BAR IS \&bar
foo	.ASSIGNA	0
	.AWHILE	\&foo LT 2
	HI BEFORE
	.AREPEAT	2
	HI MEDIUM	\&foo \&bar
	.AENDR
	HI AFTER
foo	.ASSIGNA	\&foo + 1	
	.AENDW
bar	.ASSIGNA	\&bar + 1	
	AND ITS NOW \&bar
	.AENDW
	.END


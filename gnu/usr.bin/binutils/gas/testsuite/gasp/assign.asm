
foo:	.ASSIGNC	"hello"
BAR:	.ASSIGNA	12+34

	\&foo'foo
	\&foo\&foo\&foo 
	\&foo \&foo \&foo 
	\&BAR\&bar\&BAR



	
	.END

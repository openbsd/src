use ExtUtils::MakeMaker;

WriteMakefile(
	NAME 	=> 'DB_File',
	LIBS 	=> ["-L/usr/local/lib -ldb"],
	MAN3PODS 	=> ' ', 	# Pods will be built by installman.
	#INC	=> '-I/usr/local/include',
	VERSION_FROM	=> 'DB_File.pm',
	XSPROTOARG => '-noprototypes', 		# XXX remove later?
);


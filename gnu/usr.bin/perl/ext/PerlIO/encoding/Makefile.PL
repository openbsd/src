use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "PerlIO::encoding",
	MAN3PODS    => {},  # Pods will be built by installman.
	VERSION_FROM	=> 'encoding.pm',
);


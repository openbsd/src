use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "PerlIO::via",
	MAN3PODS    => {},  # Pods will be built by installman.
	VERSION_FROM	=> 'via.pm',
);


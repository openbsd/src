use ExtUtils::MakeMaker;
WriteMakefile(
	NAME		=> "PerlIO::scalar",
	MAN3PODS    => {},  # Pods will be built by installman.
	VERSION_FROM	=> 'scalar.pm',
);


require 5.005;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME	 => 'MIME::Base64',
    MAN3PODS	 => {},  # Pods will be built by installman.
    VERSION_FROM => 'Base64.pm',
    dist         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
);

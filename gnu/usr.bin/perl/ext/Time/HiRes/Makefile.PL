# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
#

use strict;
use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'	   => 'Time::HiRes',
    MAN3PODS    => {},  # Pods will be built by installman.
    'VERSION_FROM' => 'HiRes.pm',
);


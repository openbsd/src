require 5.004;
use strict;
use Config qw(%Config);
use ExtUtils::MakeMaker;

my @extra;

unless ($Config{d_u32align}) {
    @extra = (DEFINE => "-DU32_ALIGNMENT_REQUIRED")
	if !($Config{'byteorder'} eq '1234' ||
	     $Config{'byteorder'} eq '4321');
}
my @optimize = ();
if ($^O eq 'VMS') {
    if (defined($Config{ccname})) {
        if (grep(/VMS_VAX/, @INC) && ($Config{ccname} eq 'DEC')) {
            # VAX compiler optimizer even as late as v6.4 gets stuck
            @optimize = ("OPTIMIZE","/Optimize=(NODISJOINT)");
        }
    }
}

WriteMakefile(
    'NAME'	   => 'Digest::MD5',
    'VERSION_FROM' => 'MD5.pm',
    MAN3PODS    => {},  # Pods will be built by installman.
    @extra,
    'dist'         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
    @optimize
);
exit;


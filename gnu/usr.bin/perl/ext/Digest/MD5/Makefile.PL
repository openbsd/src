require 5.004;
use strict;
use Config qw(%Config);
use ExtUtils::MakeMaker;

my @extra;
@extra = (DEFINE => "-DU32_ALIGNMENT_REQUIRED") unless free_u32_alignment();

if ($^O eq 'VMS') {
    if (defined($Config{ccname})) {
        if (grep(/VMS_VAX/, @INC) && ($Config{ccname} eq 'DEC')) {
            # VAX compiler optimizer even as late as v6.4 gets stuck
            push(@extra, OPTIMIZE => "/Optimize=(NODISJOINT)");
        }
    }
}

push(@extra, 'INSTALLDIRS'  => 'perl') if $] >= 5.008;

WriteMakefile(
    'NAME'	   => 'Digest::MD5',
    'VERSION_FROM' => 'MD5.pm',
    MAN3PODS    => {},  # Pods will be built by installman.
    @extra,
    'dist'         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz', },
);



sub free_u32_alignment
{
    return 0 if $Config{d_u32align};
    return 1 if $Config{'byteorder'} eq '1234' || $Config{'byteorder'} eq '4321';
    return 0;
}

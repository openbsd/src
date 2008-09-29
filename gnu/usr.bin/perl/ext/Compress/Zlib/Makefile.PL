#! perl -w

use strict ;
require 5.004 ;

$::VERSION = '2.008' ;

use private::MakeUtil;
use ExtUtils::MakeMaker 5.16 ;


UpDowngrade(getPerlFiles('MANIFEST')) 
    unless $ENV{PERL_CORE};

WriteMakefile( 
    NAME         => 'Compress::Zlib',
    VERSION_FROM => 'lib/Compress/Zlib.pm',
    'dist'       => { COMPRESS     => 'gzip', 
                      TARFLAGS     => '-chvf',
                      SUFFIX       => 'gz',
                      DIST_DEFAULT => 'MyTrebleCheck tardist',
                    },

    (
      $ENV{SKIP_FOR_CORE}
        ? (MAN3PODS    => {})
	    : (PREREQ_PM   => { 'Compress::Raw::Zlib'           => $::VERSION,
		                    'IO::Compress::Base'            => $::VERSION,
		                    'IO::Compress::Base::Common'    => $::VERSION,
		                    'IO::Uncompress::Base'          => $::VERSION,
		                    'IO::Compress::Gzip'            => $::VERSION,
		                    'IO::Compress::Gzip::Constants' => $::VERSION,
		                    'IO::Uncompress::Gunzip'        => $::VERSION,
		                    'Scalar::Util'                  => 0,
                          }
	      )
    ),
       
    (
    $] >= 5.005
        ? (ABSTRACT_FROM => 'lib/Compress/Zlib.pm',
            AUTHOR       => 'Paul Marquess <pmqs@cpan.org>')
        : ()
    ),

    INSTALLDIRS => ($] >= 5.009 ? 'perl' : 'site'),

    ((ExtUtils::MakeMaker->VERSION() gt '6.30') ?
        ('LICENSE'  => 'perl')         : ()),    

) ;

# Check for Compress::Zlib version 1.
eval  "require Compress::Zlib; " ;
if ( ! $ENV{PERL_CORE} && ! $@ && $Compress::Zlib::VERSION < 2)
{
    my $ver = $Compress::Zlib::VERSION ;
    print <<EOM

I see you already have Compress::Zlib version $ver installed.
Version 1 MUST be removed for version 2 to operate correctly.

The easiest way to do that is to install Compress::Zlib like this

    make install UNINST=1

If you run the CPAN shell run this before installing Compress::Zlib

    o conf make_install_arg UNINST=1

EOM
}

# end of file Makefile.PL


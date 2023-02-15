BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = ("../lib", "lib/compress");
    }
}

use lib qw(t t/compress);
use strict ;
use warnings ;

use Test::More ;

BEGIN
{
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 2 + $extra ;

    use_ok('Compress::Raw::Zlib', 2) ;
}

sub bit
{
    return 1 << $_[0];
}

{

    my $zlib_h = ZLIB_VERSION ;
    my $libz   = Compress::Raw::Zlib::zlib_version;
    my $ZLIB_VERNUM = sprintf ("0x%X", Compress::Raw::Zlib::ZLIB_VERNUM()) ;
    my $flags = Compress::Raw::Zlib::zlibCompileFlags();

    my %sizes = (
        0   => '16 bit',
        1   => '32 bit',
        2   => '64 bit',
        3   => 'other'
    );
    my $uIntSize       = $sizes{ ($flags >> 0) & 0x3 };
    my $uLongSize      = $sizes{ ($flags >> 2) & 0x3 };
    my $pointerSize    = $sizes{ ($flags >> 4) & 0x3 };
    my $zOffSize       = $sizes{ ($flags >> 6) & 0x3 };

    my @compiler_options;
    push @compiler_options, 'ZLIB_DEBUG'  if $flags & bit(8) ;
    push @compiler_options, 'ASM'         if $flags & bit(9) ;
    push @compiler_options, 'ZLIB_WINAPI' if $flags & bit(10) ;
    push @compiler_options, 'None'        unless @compiler_options;
    my $compiler_options = join ", ", @compiler_options;

    my @one_time;
    push @one_time, 'BUILDFIXED'  if $flags & bit(12) ;
    push @one_time, 'DYNAMIC_CRC_TABLE'  if $flags & bit(13) ;
    push @one_time, 'None'        unless @one_time;
    my $one_time = join ", ", @one_time;

    my @library;
    push @library, 'NO_GZCOMPRESS'  if $flags & bit(16) ;
    push @library, 'NO_GZIP'  if $flags & bit(17) ;
    push @library, 'None'        unless @library;
    my $library = join ", ", @library;

    my @operational;
    push @operational, 'PKZIP_BUG_WORKAROUND'  if $flags & bit(20) ;
    push @operational, 'FASTEST'  if $flags & bit(21) ;
    push @operational, 'None'        unless @operational;
    my $operational = join ", ", @operational;

    diag <<EOM ;


Compress::Raw::Zlib::VERSION        $Compress::Raw::Zlib::VERSION

ZLIB_VERSION (from zlib.h)          $zlib_h
zlib_version (from zlib library)    $libz

ZLIB_VERNUM                         $ZLIB_VERNUM
BUILD_ZLIB                          $Compress::Raw::Zlib::BUILD_ZLIB
GZIP_OS_CODE                        $Compress::Raw::Zlib::gzip_os_code

zlibCompileFlags                    $flags
  Type Sizes
    size of uInt                    $uIntSize
    size of uLong                   $uLongSize
    size of pointer                 $pointerSize
    size of z_off_t                 $zOffSize
  Compiler Options                  $compiler_options
  One-time table building           $one_time
  Library content                   $library
  Operation variations              $operational

EOM
}

# Check zlib_version and ZLIB_VERSION are the same.

SKIP: {
    skip "TEST_SKIP_VERSION_CHECK is set", 1
        if $ENV{TEST_SKIP_VERSION_CHECK};

    my $zlib_h = ZLIB_VERSION ;
    my $libz   = Compress::Raw::Zlib::zlib_version;

    is($zlib_h, $libz, "ZLIB_VERSION ($zlib_h) matches Compress::Raw::Zlib::zlib_version")
        or diag <<EOM;

The version of zlib.h does not match the version of libz

You have zlib.h version $zlib_h
     and libz   version $libz

You probably have two versions of zlib installed on your system.
Try removing the one you don't want to use and rebuild.
EOM
}

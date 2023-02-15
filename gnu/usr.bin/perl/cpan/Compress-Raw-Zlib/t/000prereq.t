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

    diag "Running Perl version  $]\n";

    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };


    my $VERSION = '2.202';
    my @NAMES = qw(

			);

    my @OPT = qw(

			);

    plan tests => 1 + @NAMES + @OPT + $extra ;

    ok 1;

    foreach my $name (@NAMES)
    {
        use_ok($name, $VERSION);
    }


    foreach my $name (@OPT)
    {
        eval " require $name " ;
        if ($@)
        {
            ok 1, "$name not available"
        }
        else
        {
            my $ver = eval("\$${name}::VERSION");
            is $ver, $VERSION, "$name version should be $VERSION"
                or diag "$name version is $ver, need $VERSION" ;
        }
    }

}

sub bit
{
    return 1 << $_[0];
}

{
    # Print our versions of all modules used

    use Compress::Raw::Zlib;

    my @results = ( [ 'Perl', $] ] );
    my @modules = qw(
                    Compress::Raw::Zlib
                    );

    my %have = ();

    for my $module (@modules)
    {
        my $ver = packageVer($module) ;
        my $v = defined $ver
                    ? $ver
                    : "Not Installed" ;
        push @results, [$module, $v] ;
        $have{$module} ++
            if $ver ;
    }

    push @results, ['',''];
    push @results, ["zlib_version (from zlib library)", Compress::Raw::Zlib::zlib_version() ];
    push @results, ["ZLIB_VERSION (from zlib.h)", Compress::Raw::Zlib::ZLIB_VERSION ];
    push @results, ["ZLIB_VERNUM", sprintf("0x%x", Compress::Raw::Zlib::ZLIB_VERNUM) ];
    push @results, ['',''];

    push @results, ['BUILD_ZLIB',  $Compress::Raw::Zlib::BUILD_ZLIB];
    push @results, ['GZIP_OS_CODE',  $Compress::Raw::Zlib::gzip_os_code];
    push @results, ['',''];

    if (Compress::Raw::Zlib::is_zlibng)
    {
        push @results, ["Using zlib-ng", "Yes" ];

        push @results, ["zlibng_version", Compress::Raw::Zlib::zlibng_version() ];

        if (Compress::Raw::Zlib::is_zlibng_compat)
        {
            push @results, ["zlib-ng Mode", "Compat" ];
        }
        else
        {
            push @results, ["zlib-ng Mode", "Native" ];
        }

        my @ng = qw(
            ZLIBNG_VERSION
            ZLIBNG_VER_MAJOR
            ZLIBNG_VER_MINOR
            ZLIBNG_VER_REVISION
            ZLIBNG_VER_STATUS
            ZLIBNG_VER_MODIFIED
            );

        for my $n (@ng)
        {
            no strict 'refs';
            push @results, ["  $n", &{ "Compress::Raw::Zlib::$n" } ];
        }

        no strict 'refs';
        push @results, ["  ZLIBNG_VERNUM", sprintf("0x%x", &{ "Compress::Raw::Zlib::ZLIBNG_VERNUM" }) ];

    }
    else
    {
        push @results, ["Using zlib-ng", "No" ];
    }

    push @results, ['',''];
    push @results, ["is_zlib_native",   Compress::Raw::Zlib::is_zlib_native() ? 1 : 0 ];
    push @results, ["is_zlibng",        Compress::Raw::Zlib::is_zlibng() ?1 : 0];
    push @results, ["is_zlibng_native", Compress::Raw::Zlib::is_zlibng_native() ? 1 : 0 ];
    push @results, ["is_zlibng_compat", Compress::Raw::Zlib::is_zlibng_compat() ? 1 : 0];


    my $zlib_h = ZLIB_VERSION ;
    my $libz   = Compress::Raw::Zlib::zlib_version;
    my $ZLIB_VERNUM = sprintf ("0x%X", Compress::Raw::Zlib::ZLIB_VERNUM()) ;
    my $flags = Compress::Raw::Zlib::zlibCompileFlags();

    push @results, ['',''];
    push @results, ['zlibCompileFlags', $flags];
    push @results, ['  Type Sizes', ''];

    my %sizes = (
        0   => '16 bit',
        1   => '32 bit',
        2   => '64 bit',
        3   => 'other'
    );

    push @results, ['    size of uInt',      $sizes{ ($flags >> 0) & 0x3 } ];
    push @results, ['    size of uLong',     $sizes{ ($flags >> 2) & 0x3 } ];
    push @results, ['    size of pointer',   $sizes{ ($flags >> 4) & 0x3 } ];
    push @results, ['    size of z_off_t',   $sizes{ ($flags >> 6) & 0x3 } ];

    my @compiler_options;
    push @compiler_options, 'ZLIB_DEBUG'  if $flags & bit(8) ;
    push @compiler_options, 'ASM'         if $flags & bit(9) ;
    push @compiler_options, 'ZLIB_WINAPI' if $flags & bit(10) ;
    push @compiler_options, 'None'        unless @compiler_options;
    push @results, ['  Compiler Options', join ", ", @compiler_options];

    my @one_time;
    push @one_time, 'BUILDFIXED'  if $flags & bit(12) ;
    push @one_time, 'DYNAMIC_CRC_TABLE'  if $flags & bit(13) ;
    push @one_time, 'None'        unless @one_time;
    push @results, ['  One-time table building', join ", ", @one_time];

    my @library;
    push @library, 'NO_GZCOMPRESS'  if $flags & bit(16) ;
    push @library, 'NO_GZIP'  if $flags & bit(17) ;
    push @library, 'None'        unless @library;
    push @results, ['  Library content', join ", ", @library];

    my @operational;
    push @operational, 'PKZIP_BUG_WORKAROUND'  if $flags & bit(20) ;
    push @operational, 'FASTEST'  if $flags & bit(21) ;
    push @operational, 'None'        unless @operational;
    push @results, ['  Operation variations', join ", ", @operational];



    if ($have{"Compress::Raw::Lzma"})
    {
        my $ver = eval { Compress::Raw::Lzma::lzma_version_string(); } || "unknown";
        push @results, ["lzma", $ver] ;
    }

    use List::Util qw(max);
    my $width = max map { length $_->[0] } @results;

    diag "\n\n" ;
    for my $m (@results)
    {
        my ($name, $ver) = @$m;

        my $b = " " x (1 + $width - length $name);

        diag $name . $b . $ver . "\n" ;
    }

    diag "\n\n" ;
}

sub packageVer
{
    no strict 'refs';
    my $package = shift;

    eval "use $package;";
    return ${ "${package}::VERSION" };

}
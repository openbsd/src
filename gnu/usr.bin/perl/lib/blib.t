#!./perl -Tw

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;
use File::Spec;
my($blib, $blib_arch, $blib_lib, @blib_dirs);

sub _cleanup {
    rmdir foreach reverse (@_);
    unlink "stderr" unless $^O eq 'MacOS';
}

sub _mkdirs {
    for my $dir (@_) {
        next if -d $dir;
        mkdir $dir or die "Can't mkdir $dir: $!" if ! -d $dir;
    }
}
    

BEGIN {
    if ($^O eq 'MacOS')
    {
	$MacPerl::Architecture = $MacPerl::Architecture; # shhhhh
	$blib = ":blib:";
	$blib_lib = ":blib:lib:";
	$blib_arch = ":blib:lib:$MacPerl::Architecture:";
	@blib_dirs = ($blib, $blib_lib, $blib_arch); # order
    }
    else
    {
	$blib = "blib";
	$blib_arch = "blib/arch";
	$blib_lib = "blib/lib";
	@blib_dirs = ($blib, $blib_arch, $blib_lib);
    }
    _cleanup( @blib_dirs );
}

use Test::More tests => 7;

eval 'use blib;';
ok( $@ =~ /Cannot find blib/, 'Fails if blib directory not found' );

_mkdirs( @blib_dirs );

{
    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings = join '', @_ };
    use_ok('blib');
    is( $warnings, '',  'use blib is nice and quiet' );
}

is( @INC, 3, '@INC now has 3 elements' );
is( $INC[2],    '../lib',       'blib added to the front of @INC' );

if ($^O eq 'VMS') {
    # Unix syntax is accepted going in but it's not what comes out
    # So we don't use catdir above
    $blib_arch = 'blib.arch]';
    $blib_lib = 'blib.lib]';
}
elsif ($^O ne 'MacOS')
{
    $blib_arch = File::Spec->catdir("blib","arch");
    $blib_lib  = File::Spec->catdir("blib","lib");
}


ok( grep(m|\Q$blib_lib\E$|, @INC[0,1])  == 1,     "  $blib_lib in \@INC");
ok( grep(m|\Q$blib_arch\E$|, @INC[0,1]) == 1,     "  $blib_arch in \@INC");

END { _cleanup( @blib_dirs ); }

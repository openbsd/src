#!./perl

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
	require Config; import Config;
}

use Test::More tests => 17;

# open::import expects 'open' as its first argument, but it clashes with open()
sub import {
	open::import( 'open', @_ );
}

# can't use require_ok() here, with a name like 'open'
ok( require 'open.pm', 'requiring open' );

# this should fail
eval { import() };
like( $@, qr/needs explicit list of PerlIO layers/,
	'import should fail without args' );

# the hint bits shouldn't be set yet
is( $^H & $open::hint_bits, 0,
	'hint bits should not be set in $^H before open import' );

# prevent it from loading I18N::Langinfo, so we can test encoding failures
my $warn;
local $SIG{__WARN__} = sub {
	$warn .= shift;
};

# and it shouldn't be able to find this layer
$warn = '';
eval q{ no warnings 'layer'; use open IN => ':macguffin' ; };
is( $warn, '',
	'should not warn about unknown layer with bad layer provided' );

$warn = '';
eval q{ use warnings 'layer'; use open IN => ':macguffin' ; };
like( $warn, qr/Unknown PerlIO layer/,
	'should warn about unknown layer with bad layer provided' );

SKIP: {
    skip("no perlio, no :utf8", 1) unless (find PerlIO::Layer 'perlio');
    skip("no Encode for locale layer", 1) unless eval { require Encode }; 
    # now load a real-looking locale
    $ENV{LC_ALL} = ' .utf8';
    import( 'IN', 'locale' );
    like( ${^OPEN}, qr/^(:utf8)?:utf8\0/,
        'should set a valid locale layer' );
}

# and see if it sets the magic variables appropriately
import( 'IN', ':crlf' );
ok( $^H & $open::hint_bits,
	'hint bits should be set in $^H after open import' );
is( $^H{'open_IN'}, 'crlf', 'should have set crlf layer' );

# it should reset them appropriately, too
import( 'IN', ':raw' );
is( $^H{'open_IN'}, 'raw', 'should have reset to raw layer' );

# it dies if you don't set IN, OUT, or IO
eval { import( 'sideways', ':raw' ) };
like( $@, qr/Unknown PerlIO layer class/, 'should croak with unknown class' );

# but it handles them all so well together
import( 'IO', ':raw :crlf' );
is( ${^OPEN}, ":raw :crlf\0:raw :crlf",
	'should set multi types, multi layer' );
is( $^H{'open_IO'}, 'crlf', 'should record last layer set in %^H' );

SKIP: {
    skip("no perlio, no :utf8", 4) unless (find PerlIO::Layer 'perlio');

    eval <<EOE;
    use open ':utf8';
    open(O, ">utf8");
    print O chr(0x100);
    close O;
    open(I, "<utf8");
    is(ord(<I>), 0x100, ":utf8 single wide character round-trip");
    close I;
EOE

    open F, ">a";
    @a = map { chr(1 << ($_ << 2)) } 0..5; # 0x1, 0x10, .., 0x100000
    unshift @a, chr(0); # ... and a null byte in front just for fun
    print F @a;
    close F;

    sub systell {
        use Fcntl 'SEEK_CUR';
        sysseek($_[0], 0, SEEK_CUR);
    }

    require bytes; # not use

    my $ok;

    open F, "<:utf8", "a";
    $ok = $a = 0;
    for (@a) {
        unless (
		($c = sysread(F, $b, 1)) == 1  &&
		length($b)               == 1  &&
		ord($b)                  == ord($_) &&
		systell(F)               == ($a += bytes::length($b))
		) {
	    print '# ord($_)           == ', ord($_), "\n";
	    print '# ord($b)           == ', ord($b), "\n";
	    print '# length($b)        == ', length($b), "\n";
	    print '# bytes::length($b) == ', bytes::length($b), "\n";
	    print '# systell(F)        == ', systell(F), "\n";
	    print '# $a                == ', $a, "\n";
	    print '# $c                == ', $c, "\n";
	    last;
	}
	$ok++;
    }
    close F;
    ok($ok == @a,
       "on :utf8 streams sysread() should work on characters, not bytes");

    # syswrite() on should work on characters, not bytes
    open G, ">:utf8", "b";
    $ok = $a = 0;
    for (@a) {
	unless (
		($c = syswrite(G, $_, 1)) == 1 &&
		systell(G)                == ($a += bytes::length($_))
		) {
	    print '# ord($_)           == ', ord($_), "\n";
	    print '# bytes::length($_) == ', bytes::length($_), "\n";
	    print '# systell(G)        == ', systell(G), "\n";
	    print '# $a                == ', $a, "\n";
	    print '# $c                == ', $c, "\n";
	    print "not ";
	    last;
	}
	$ok++;
    }
    close G;
    ok($ok == @a,
       "on :utf8 streams syswrite() should work on characters, not bytes");

    open G, "<:utf8", "b";
    $ok = $a = 0;
    for (@a) {
	unless (
		($c = sysread(G, $b, 1)) == 1 &&
		length($b)               == 1 &&
		ord($b)                  == ord($_) &&
		systell(G)               == ($a += bytes::length($_))
		) {
	    print '# ord($_)           == ', ord($_), "\n";
	    print '# ord($b)           == ', ord($b), "\n";
	    print '# length($b)        == ', length($b), "\n";
	    print '# bytes::length($b) == ', bytes::length($b), "\n";
	    print '# systell(G)        == ', systell(G), "\n";
	    print '# $a                == ', $a, "\n";
	    print '# $c                == ', $c, "\n";
	    last;
	}
	$ok++;
    }
    close G;
    ok($ok == @a,
       "checking syswrite() output on :utf8 streams by reading it back in");
}

SKIP: {
    skip("no perlio", 1) unless (find PerlIO::Layer 'perlio');
    use open IN => ':non-existent';
    eval {
	require Symbol; # Anything that exists but we havn't loaded
    };
    like($@, qr/Can't locate Symbol|Recursive call/i,
	 "test for an endless loop in PerlIO_find_layer");
}

END {
    1 while unlink "utf8";
    1 while unlink "a";
    1 while unlink "b";
}

# the test cases beyond __DATA__ need to be executed separately

__DATA__
$ENV{LC_ALL} = 'nonexistent.euc';
eval { open::_get_locale_encoding() };
like( $@, qr/too ambiguous/, 'should die with ambiguous locale encoding' );
%%%
# the special :locale layer
$ENV{LC_ALL} = $ENV{LANG} = 'ru_RU.KOI8-R';
# the :locale will probe the locale environment variables like LANG
use open OUT => ':locale';
open(O, ">koi8");
print O chr(0x430); # Unicode CYRILLIC SMALL LETTER A = KOI8-R 0xc1
close O;
open(I, "<koi8");
printf "%#x\n", ord(<I>), "\n"; # this should print 0xc1
close I;
%%%

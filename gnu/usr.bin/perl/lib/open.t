#!./perl

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	require './test.pl';
}

plan 23;

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

# open :locale logic changed since open 1.04, new logic
# difficult to test portably.

# see if it sets the magic variables appropriately
import( 'IN', ':crlf' );
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
    skip("no perlio, no :utf8", 12) unless (find PerlIO::Layer 'perlio');

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

    sub diagnostics {
	print '# ord($_)           == ', ord($_), "\n";
	print '# bytes::length($_) == ', bytes::length($_), "\n";
	print '# systell(G)        == ', systell(G), "\n";
	print '# $a                == ', $a, "\n";
	print '# $c                == ', $c, "\n";
    }


    my %actions = (
		   syswrite => sub { syswrite G, shift; },
		   'syswrite len' => sub { syswrite G, shift, 1; },
		   'syswrite len pad' => sub {
		       my $temp = shift() . "\243";
		       syswrite G, $temp, 1; },
		   'syswrite off' => sub { 
		       my $temp = "\351" . shift();
		       syswrite G, $temp, 1, 1; },
		   'syswrite off pad' => sub { 
		       my $temp = "\351" . shift() . "\243";
		       syswrite G, $temp, 1, 1; },
		  );

    foreach my $key (sort keys %actions) {
	# syswrite() on should work on characters, not bytes
	open G, ">:utf8", "b";

	print "# $key\n";
	$ok = $a = 0;
	for (@a) {
	    unless (
		    ($c = $actions{$key}($_)) == 1 &&
		    systell(G)                == ($a += bytes::length($_))
		   ) {
		diagnostics();
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
}
SKIP: {
    skip("no perlio", 2) unless (find PerlIO::Layer 'perlio');
    skip("no Encode", 2) unless $Config{extensions} =~ m{\bEncode\b};

    eval q[use Encode::Alias;use open ":std", ":locale"];
    is($@, '', 'can use :std and :locale');
}

{
    local $ENV{PERL_UNICODE};
    delete $ENV{PERL_UNICODE};
    is runperl(
         progs => [
            'use open q\:encoding(UTF-8)\, q-:std-;',
            'use open q\:encoding(UTF-8)\;',
            'if(($_ = <STDIN>) eq qq-\x{100}\n-) { print qq-stdin ok\n- }',
            'else { print qq-got -, join(q q q, map ord, split//), "\n" }',
            'print STDOUT qq-\x{ff}\n-;',
            'print STDERR qq-\x{ff}\n-;',
         ],
         stdin => "\xc4\x80\n",
         stderr => 1,
       ),
       "stdin ok\n\xc3\xbf\n\xc3\xbf\n",
       "use open without :std does not affect standard handles",
    ;
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


BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 20;

my $a = chr(0x100);

is(ord($a), 0x100, "ord sanity check");
is(length($a), 1,  "length sanity check");
is(substr($a, 0, 1), "\x{100}",  "substr sanity check");
is(index($a, "\x{100}"),  0, "index sanity check");
is(rindex($a, "\x{100}"), 0, "rindex sanity check");
is(bytes::length($a), 2,  "bytes::length sanity check");
is(bytes::chr(0x100), chr(0),  "bytes::chr sanity check");

{
    use bytes;
    my $b = chr(0x100); # affected by 'use bytes'
    is(ord($b), 0, "chr truncates under use bytes");
    is(length($b), 1, "length truncated under use bytes");
    is(bytes::ord($b), 0, "bytes::ord truncated under use bytes");
    is(bytes::length($b), 1, "bytes::length truncated under use bytes");
    is(bytes::substr($b, 0, 1), "\0", "bytes::substr truncated under use bytes");
}

my $c = chr(0x100);

{
    use bytes;
    if (ord('A') == 193) { # EBCDIC?
	is(ord($c), 0x8c, "ord under use bytes looks at the 1st byte");
    } else {
	is(ord($c), 0xc4, "ord under use bytes looks at the 1st byte");
    }
    is(length($c), 2, "length under use bytes looks at bytes");
    is(bytes::length($c), 2, "bytes::length under use bytes looks at bytes");
    if (ord('A') == 193) { # EBCDIC?
	is(bytes::ord($c), 0x8c, "bytes::ord under use bytes looks at the 1st byte");
    } else {
	is(bytes::ord($c), 0xc4, "bytes::ord under use bytes looks at the 1st byte");
    }
    # In z/OS \x41,\x8c are the codepoints corresponding to \x80,\xc4 respectively under ASCII platform
    if (ord('A') == 193) { # EBCDIC?
        is(bytes::substr($c, 0, 1), "\x8c", "bytes::substr under use bytes looks at bytes");
        is(bytes::index($c, "\x41"), 1, "bytes::index under use bytes looks at bytes");
        is(bytes::rindex($c, "\x8c"), 0, "bytes::rindex under use bytes looks at bytes");

    }
    else{
        is(bytes::substr($c, 0, 1), "\xc4", "bytes::substr under use bytes looks at bytes");
        is(bytes::index($c, "\x80"), 1, "bytes::index under use bytes looks at bytes");
        is(bytes::rindex($c, "\xc4"), 0, "bytes::rindex under use bytes looks at bytes");
    }
    
}

{
    fresh_perl_like ('use bytes; bytes::moo()',
		     qr/Undefined subroutine bytes::moo/, {stderr=>1},
		    "Check Carp is loaded for AUTOLOADing errors")
}

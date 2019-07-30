#!./perl -w
# tests that utf8_heavy.pl doesn't use anything that prevents it loading
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 1;

# see [perl #126593]
fresh_perl_is(<<'EOP', "", { stderr => 1 }, "doesn't break with \${^ENCODING}");
no warnings qw(deprecated);
package Foo;
sub cat_decode {
    # stolen from Encode.pm
    my ( undef, undef, undef, $pos, $trm ) = @_;
    my ( $rdst, $rsrc, $rpos ) = \@_[ 1, 2, 3 ];
    use bytes;
    if ( ( my $npos = index( $$rsrc, $trm, $pos ) ) >= 0 ) {
        $$rdst .=
          substr( $$rsrc, $pos, $npos - $pos + length($trm) );
        $$rpos = $npos + length($trm);
        return 1;
    }
    $$rdst .= substr( $$rsrc, $pos );
    $$rpos = length($$rsrc);
    return q();
}

sub decode {
   my (undef, $tmp) = @_;
   utf8::decode($tmp);
   $tmp;
}

BEGIN { ${^ENCODING} = bless [], q(Foo) };

(my $tmp = q(abc)) =~ tr/abc/123/;
EOP

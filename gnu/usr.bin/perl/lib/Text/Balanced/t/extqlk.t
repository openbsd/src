BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = qw(../lib);
    }
}

#! /usr/local/bin/perl -ws
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..89\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( extract_quotelike );
$loaded = 1;
print "ok 1\n";
$count=2;
use vars qw( $DEBUG );
# $DEBUG=1;
sub debug { print "\t>>>",@_ if $DEBUG }

######################### End of black magic.


$cmd = "print";
$neg = 0;
while (defined($str = <DATA>))
{
	chomp $str;
	if ($str =~ s/\A# USING://) { $neg = 0; $cmd = $str; next; }
	elsif ($str =~ /\A# TH[EI]SE? SHOULD FAIL/) { $neg = 1; next; }
	elsif (!$str || $str =~ /\A#/) { $neg = 0; next }
	debug "\tUsing: $cmd\n";
	debug "\t   on: [$str]\n";
	$str =~ s/\\n/\n/g;
	my $orig = $str;

	 my @res;
	eval qq{\@res = $cmd; };
	debug "\t  got:\n" . join "", map { $res[$_]=~s/\n/\\n/g; "\t\t\t$_: [$res[$_]]\n"} (0..$#res);
	debug "\t left: " . (map { s/\n/\\n/g; "[$_]\n" } my $cpy1 = $str)[0];
	debug "\t  pos: " . (map { s/\n/\\n/g; "[$_]\n" } my $cpy2 = substr($str,pos($str)))[0] . "...]\n";
	print "not " if (substr($str,pos($str),1) eq ';')==$neg;
	print "ok ", $count++;
	print "\n";

	$str = $orig;
	debug "\tUsing: scalar $cmd\n";
	debug "\t   on: [$str]\n";
	$var = eval $cmd;
	print " ($@)" if $@ && $DEBUG;
	$var = "<undef>" unless defined $var;
	debug "\t scalar got: " . (map { s/\n/\\n/g; "[$_]\n" } $var)[0];
	debug "\t scalar left: " . (map { s/\n/\\n/g; "[$_]\n" } $str)[0];
	print "not " if ($str =~ '\A;')==$neg;
	print "ok ", $count++;
	print "\n";
}

__DATA__

# USING: extract_quotelike($str);
'';
"";
"a";
'b';
`cc`;


<<EOHERE; done();\nline1\nline2\nEOHERE\n; next;
     <<EOHERE; done();\nline1\nline2\nEOHERE\n; next;
<<"EOHERE"; done()\nline1\nline2\nEOHERE\n and next
<<`EOHERE`; done()\nline1\nline2\nEOHERE\n and next
<<'EOHERE'; done()\nline1\n'line2'\nEOHERE\n and next
<<'EOHERE;'; done()\nline1\nline2\nEOHERE;\n and next
<<"   EOHERE"; done() \nline1\nline2\n   EOHERE\nand next
<<""; done()\nline1\nline2\n\n and next
<<; done()\nline1\nline2\n\n and next


"this is a nested $var[$x] {";
/a/gci;
m/a/gci;

q(d);
qq(e);
qx(f);
qr(g);
qw(h i j);
q{d};
qq{e};
qx{f};
qr{g};
qq{a nested { and } are okay as are () and <> pairs and escaped \}'s };
q/slash/;
q # slash #;
qr qw qx;

s/x/y/;
s/x/y/cgimsox;
s{a}{b};
s{a}\n {b};
s(a){b};
s(a)/b/;
s/'/\\'/g;
tr/x/y/;
y/x/y/;

# THESE SHOULD FAIL
s<$self->{pat}>{$self->{sub}};		# CAN'T HANDLE '>' in '->'
s-$self->{pap}-$self->{sub}-;		# CAN'T HANDLE '-' in '->'
<<EOHERE; done();\nline1\nline2\nEOHERE;\n; next;	    # RDEL HAS NO ';'
<<'EOHERE'; done();\nline1\nline2\nEOHERE;\n; next;	    # RDEF HAS NO ';'
     <<    EOTHERE; done();\nline1\nline2\n    EOTHERE\n; next;  # RDEL IS "" (!)

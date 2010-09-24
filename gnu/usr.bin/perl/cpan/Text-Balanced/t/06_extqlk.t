#! /usr/local/bin/perl -ws
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..95\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( extract_quotelike );
$loaded = 1;
print "ok 1\n";
$count=2;
use vars qw( $DEBUG );
#$DEBUG=1;
sub debug { print "\t>>>",@_ if $ENV{DEBUG} }
sub esc   { my $x = shift||'<undef>'; $x =~ s/\n/\\n/gs; $x }

######################### End of black magic.


$cmd = "print";
$neg = 0;
while (defined($str = <DATA>))
{
	chomp $str;
	if ($str =~ s/\A# USING://)                 { $neg = 0; $cmd = $str; next; }
	elsif ($str =~ /\A# TH[EI]SE? SHOULD FAIL/) { $neg = 1; next; }
	elsif (!$str || $str =~ /\A#/)              { $neg = 0; next }
	my $setup_cmd = ($str =~ s/\A\{(.*)\}//) ? $1 : '';
	my $tests = 'sl';
	$str =~ s/\\n/\n/g;
	my $orig = $str;

	eval $setup_cmd if $setup_cmd ne ''; 
	if($tests =~ /l/) {
		debug "\tUsing: $cmd\n";
		debug "\t   on: [" . esc($setup_cmd) . "][" . esc($str) . "]\n";
		my @res;
		eval qq{\@res = $cmd; };
		debug "\t  got:\n" . join "", map { "\t\t\t$_: [" . esc($res[$_]) . "]\n"} (0..$#res);
		debug "\t left: [" . esc($str) . "]\n";
		debug "\t  pos: [" . esc(substr($str,pos($str))) . "...]\n";
		print "not " if (substr($str,pos($str),1) eq ';')==$neg;
		print "ok ", $count++;
		print "\n";
	}

	eval $setup_cmd if $setup_cmd ne '';
	if($tests =~ /s/) {
		$str = $orig;
		debug "\tUsing: scalar $cmd\n";
		debug "\t   on: [" . esc($str) . "]\n";
		$var = eval $cmd;
		print " ($@)" if $@ && $DEBUG;
		$var = "<undef>" unless defined $var;
		debug "\t scalar got: [" . esc($var) . "]\n";
		debug "\t scalar left: [" . esc($str) . "]\n";
		print "not " if ($str =~ '\A;')==$neg;
		print "ok ", $count++;
		print "\n";
	}
}

# fails in Text::Balanced 1.95
$_ = qq(s{}{});
my @z = extract_quotelike();
print "not " if $z[0] eq '';
print "ok ", $count++;
print "\n";

 
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
# fails in Text::Balanced 1.95
<<EOHERE;\nEOHERE\n; 
# fails in Text::Balanced 1.95
<<"*";\n\n*\n; 

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

# fails on Text-Balanced-1.95
{ $tests = 'l'; pos($str)=6 }012345<<E;\n\nE\n

# THESE SHOULD FAIL
s<$self->{pat}>{$self->{sub}};		# CAN'T HANDLE '>' in '->'
s-$self->{pap}-$self->{sub}-;		# CAN'T HANDLE '-' in '->'
<<EOHERE; done();\nline1\nline2\nEOHERE;\n; next;	    # RDEL HAS NO ';'
<<'EOHERE'; done();\nline1\nline2\nEOHERE;\n; next;	    # RDEF HAS NO ';'
     <<    EOTHERE; done();\nline1\nline2\n    EOTHERE\n; next;  # RDEL IS "" (!)

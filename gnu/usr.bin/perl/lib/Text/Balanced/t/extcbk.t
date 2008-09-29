BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = qw(../lib);
    }
}

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..41\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( extract_codeblock );
$loaded = 1;
print "ok 1\n";
$count=2;
use vars qw( $DEBUG );
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
	$str =~ s/\\n/\n/g;
	debug "\tUsing: $cmd\n";
	debug "\t   on: [$str]\n";

	my @res;
	$var = eval "\@res = $cmd";
	debug "\t   Failed: $@ at " . $@+0 .")" if $@;
	debug "\t list got: [" . join("|", map {defined $_ ? $_ : '<undef>'} @res) . "]\n";
	debug "\t list left: [$str]\n";
	print "not " if (substr($str,pos($str)||0,1) eq ';')==$neg;
	print "ok ", $count++;
	print "\n";

	pos $str = 0;
	$var = eval $cmd;
	$var = "<undef>" unless defined $var;
	debug "\t scalar got: [$var]\n";
	debug "\t scalar left: [$str]\n";
	print "not " if ($str =~ '\A;')==$neg;
	print "ok ", $count++;
	print " ($@)" if $@ && $DEBUG;
	print "\n";
}

__DATA__

# USING: extract_codeblock($str,'(){}',undef,'()');
(Foo(')'));

# USING: extract_codeblock($str);
{ $data[4] =~ /['"]/; };

# USING: extract_codeblock($str,'<>');
< %x = ( try => "this") >;
< %x = () >;
< %x = ( $try->{this}, "too") >;
< %'x = ( $try->{this}, "too") >;
< %'x'y = ( $try->{this}, "too") >;
< %::x::y = ( $try->{this}, "too") >;

# THIS SHOULD FAIL
< %x = do { $try > 10 } >;

# USING: extract_codeblock($str);

{ $a = /\}/; };
{ sub { $_[0] /= $_[1] } };  # / here
{ 1; };
{ $a = 1; };


# USING: extract_codeblock($str,undef,'=*');
========{$a=1};

# USING: extract_codeblock($str,'{}<>');
< %x = do { $try > 10 } >;

# USING: extract_codeblock($str,'{}',undef,'<>');
< %x = do { $try > 10 } >;

# USING: extract_codeblock($str,'{}');
{ $a = $b; # what's this doing here? \n };'
{ $a = $b; \n $a =~ /$b/; \n @a = map /\s/ @b };

# THIS SHOULD FAIL
{ $a = $b; # what's this doing here? };'
{ $a = $b; # what's this doing here? ;'

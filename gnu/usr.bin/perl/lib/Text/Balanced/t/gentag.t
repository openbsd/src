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

BEGIN { $| = 1; print "1..37\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( gen_extract_tagged );
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
	$str =~ s/\\n/\n/g;
	if ($str =~ s/\A# USING://)
	{
		$neg = 0;
		eval{local$^W;*f = eval $str || die};
		next;
	}
	elsif ($str =~ /\A# TH[EI]SE? SHOULD FAIL/) { $neg = 1; next; }
	elsif (!$str || $str =~ /\A#/) { $neg = 0; next }
	$str =~ s/\\n/\n/g;
	debug "\tUsing: $cmd\n";
	debug "\t   on: [$str]\n";

	my @res;
	$var = eval { @res = f($str) };
	debug "\t list got: [" . join("|",@res) . "]\n";
	debug "\t list left: [$str]\n";
	print "not " if (substr($str,pos($str)||0,1) eq ';')==$neg;
	print "ok ", $count++;
	print " ($@)" if $@ && $DEBUG;
	print "\n";

	pos $str = 0;
	$var = eval { scalar f($str) };
	$var = "<undef>" unless defined $var;
	debug "\t scalar got: [$var]\n";
	debug "\t scalar left: [$str]\n";
	print "not " if ($str =~ '\A;')==$neg;
	print "ok ", $count++;
	print " ($@)" if $@ && $DEBUG;
	print "\n";
}

__DATA__

# USING: gen_extract_tagged('{','}');
	{ a test };

# USING: gen_extract_tagged(qr/<[A-Z]+>/,undef, undef, {ignore=>["<BR>"]});
	<A>aaa<B>bbb<BR>ccc</B>ddd</A>;

# USING: gen_extract_tagged("BEGIN","END");
	BEGIN at the BEGIN keyword and END at the END;
	BEGIN at the beginning and end at the END;

# USING: gen_extract_tagged(undef,undef,undef,{ignore=>["<[^>]*/>"]});
	<A>aaa<B>bbb<BR/>ccc</B>ddd</A>;

# USING: gen_extract_tagged(";","-",undef,{reject=>[";"],fail=>"MAX"});
	; at the ;-) keyword

# USING: gen_extract_tagged("<[A-Z]+>",undef, undef, {ignore=>["<BR>"]});
	<A>aaa<B>bbb<BR>ccc</B>ddd</A>;

# THESE SHOULD FAIL
	BEGIN at the beginning and end at the end;
	BEGIN at the BEGIN keyword and END at the end;

# TEST EXTRACTION OF TAGGED STRINGS
# USING: gen_extract_tagged("BEGIN","END",undef,{reject=>["BEGIN","END"]});
# THESE SHOULD FAIL
	BEGIN at the BEGIN keyword and END at the end;

# USING: gen_extract_tagged(";","-",undef,{reject=>[";"],fail=>"PARA"});
	; at the ;-) keyword


# USING: gen_extract_tagged();
	<A>some text</A>;
	<B>some text<A>other text</A></B>;
	<A>some text<A>other text</A></A>;
	<A HREF="#section2">some text</A>;

# THESE SHOULD FAIL
	<A>some text
	<A>some text<A>other text</A>;
	<B>some text<A>other text</B>;

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

BEGIN { $| = 1; print "1..19\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( extract_bracketed );
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

	$var = eval "() = $cmd";
	debug "\t list got: [$var]\n";
	debug "\t list left: [$str]\n";
	print "not " if (substr($str,pos($str),1) eq ';')==$neg;
	print "ok ", $count++;
	print " ($@)" if $@ && $DEBUG;
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

# USING: extract_bracketed($str);
{a nested { and } are okay as are () and <> pairs and escaped \}'s };
{a nested\n{ and } are okay as are\n() and <> pairs and escaped \}'s };

# USING: extract_bracketed($str,'{}');
{a nested { and } are okay as are unbalanced ( and < pairs and escaped \}'s };

# THESE SHOULD FAIL
{an unmatched nested { isn't okay, nor are ( and < };
{an unbalanced nested [ even with } and ] to match them;


# USING: extract_bracketed($str,'<"`q>');
<a q{uoted} ">" unbalanced right bracket of /(q>)/ either sort (`>>>""">>>>`) is okay >;

# USING: extract_bracketed($str,'<">');
<a quoted ">" unbalanced right bracket is okay >;

# USING: extract_bracketed($str,'<"`>');
<a quoted ">" unbalanced right bracket of either sort (`>>>""">>>>`) is okay >;

# THIS SHOULD FAIL
<a misquoted '>' unbalanced right bracket is bad >;

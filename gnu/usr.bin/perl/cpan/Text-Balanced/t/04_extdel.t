# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

use 5.008001;

use strict;
use warnings;

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

my $loaded = 0;
BEGIN { $| = 1; print "1..45\n"; }
END {print "not ok 1\n" unless $loaded;}
use Text::Balanced qw ( extract_delimited );
$loaded = 1;
print "ok 1\n";
my $count=2;
use vars qw( $DEBUG );
sub debug { print "\t>>>",@_ if $DEBUG }

######################### End of black magic.

## no critic (BuiltinFunctions::ProhibitStringyEval)

my $cmd = "print";
my $neg = 0;
my $str;
while (defined($str = <DATA>))
{
    chomp $str;
    if ($str =~ s/\A# USING://) { $neg = 0; $cmd = $str; next; }
    elsif ($str =~ /\A# TH[EI]SE? SHOULD FAIL/) { $neg = 1; next; }
    elsif (!$str || $str =~ /\A#/) { $neg = 0; next }
    $str =~ s/\\n/\n/g;
    debug "\tUsing: $cmd\n";
    debug "\t   on: [$str]\n";

    my $var = eval "() = $cmd";
    debug "\t list got: [$var]\n";
    debug "\t list left: [$str]\n";
    print "not " if (substr($str,pos($str)||0,1) eq ';')==$neg;
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
# USING: extract_delimited($str,'/#$',undef,'/#$');
/a/;
/a///;
#b#;
#b###;
$c$;
$c$$$;

# TEST EXTRACTION OF DELIMITED TEXT WITH ESCAPES
# USING: extract_delimited($str,'/#$',undef,'\\');
/a/;
/a\//;
#b#;
#b\##;
$c$;
$c\$$;

# TEST EXTRACTION OF DELIMITED TEXT
# USING: extract_delimited($str);
'a';
"b";
`c`;
'a\'';
'a\\';
'\\a';
"a\\";
"\\a";
"b\'\"\'";
`c '\`abc\`'`;

# TEST EXTRACTION OF DELIMITED TEXT
# USING: extract_delimited($str,'/#$','-->');
-->/a/;
-->#b#;
-->$c$;

# THIS SHOULD FAIL
$c$;

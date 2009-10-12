#!./perl

# The tests are in a separate file 't/op/re_tests'.
# Each line in that file is a separate test.
# There are five columns, separated by tabs.
#
# Column 1 contains the pattern, optionally enclosed in C<''>.
# Modifiers can be put after the closing C<'>.
#
# Column 2 contains the string to be matched.
#
# Column 3 contains the expected result:
# 	y	expect a match
# 	n	expect no match
# 	c	expect an error
#	T	the test is a TODO (can be combined with y/n/c)
#	B	test exposes a known bug in Perl, should be skipped
#	b	test exposes a known bug in Perl, should be skipped if noamp
#	t	test exposes a bug with threading, TODO if qr_embed_thr
#
# Columns 4 and 5 are used only if column 3 contains C<y> or C<c>.
#
# Column 4 contains a string, usually C<$&>.
#
# Column 5 contains the expected result of double-quote
# interpolating that string after the match, or start of error message.
#
# Column 6, if present, contains a reason why the test is skipped.
# This is printed with "skipped", for harness to pick up.
#
# \n in the tests are interpolated, as are variables of the form ${\w+}.
#
# Blanks lines are treated as PASSING tests to keep the line numbers
# linked to the test number.
#
# If you want to add a regular expression test that can't be expressed
# in this format, don't add it here: put it in op/pat.t instead.
#
# Note that columns 2,3 and 5 are all enclosed in double quotes and then
# evalled; so something like a\"\x{100}$1 has length 3+length($1).

my $file;
BEGIN {
    $iters = shift || 1;	# Poor man performance suite, 10000 is OK.

    # Do this open before any chdir
    $file = shift;
    if (defined $file) {
	open TESTS, $file or die "Can't open $file";
    }

    chdir 't' if -d 't';
    @INC = '../lib';

    if ($qr_embed_thr) {
	require Config;
	if (!$Config::Config{useithreads}) {
	    print "1..0 # Skip: no ithreads\n";
		exit 0;
	}
	if ($ENV{PERL_CORE_MINITEST}) {
	    print "1..0 # Skip: no dynamic loading on miniperl, no threads\n";
		exit 0;
	}
	require threads;
    }
}

use strict;
use warnings FATAL=>"all";
use vars qw($iters $numtests $bang $ffff $nulnul $OP);
use vars qw($qr $skip_amp $qr_embed $qr_embed_thr); # set by our callers


if (!defined $file) {
    open(TESTS,'op/re_tests') || open(TESTS,'t/op/re_tests')
	|| open(TESTS,':op:re_tests') || die "Can't open re_tests";
}

my @tests = <TESTS>;

close TESTS;

$bang = sprintf "\\%03o", ord "!"; # \41 would not be portable.
$ffff  = chr(0xff) x 2;
$nulnul = "\0" x 2;
$OP = $qr ? 'qr' : 'm';

$| = 1;
printf "1..%d\n# $iters iterations\n", scalar @tests;

my $test;
TEST:
foreach (@tests) {
    $test++;
    if (!/\S/ || /^\s*#/ || /^__END__$/) {
        print "ok $test # (Blank line or comment)\n";
        if (/#/) { print $_ };
        next;
    }
    chomp;
    s/\\n/\n/g;
    my ($pat, $subject, $result, $repl, $expect, $reason) = split(/\t/,$_,6);
    $reason = '' unless defined $reason;
    my $input = join(':',$pat,$subject,$result,$repl,$expect);
    # the double '' below keeps simple syntax highlighters from going crazy
    $pat = "'$pat'" unless $pat =~ /^[:''\/]/; 
    $pat =~ s/(\$\{\w+\})/$1/eeg;
    $pat =~ s/\\n/\n/g;
    $subject = eval qq("$subject"); die $@ if $@;
    $expect  = eval qq("$expect"); die $@ if $@;
    $expect = $repl = '-' if $skip_amp and $input =~ /\$[&\`\']/;
    my $todo_qr = $qr_embed_thr && ($result =~ s/t//);
    my $skip = ($skip_amp ? ($result =~ s/B//i) : ($result =~ s/B//));
    $reason = 'skipping $&' if $reason eq  '' && $skip_amp;
    $result =~ s/B//i unless $skip;
    my $todo= $result =~ s/T// ? " # TODO" : "";
    

    for my $study ('', 'study $subject', 'utf8::upgrade($subject)',
		   'utf8::upgrade($subject); study $subject') {
	# Need to make a copy, else the utf8::upgrade of an alreay studied
	# scalar confuses things.
	my $subject = $subject;
	my $c = $iters;
	my ($code, $match, $got);
        if ($repl eq 'pos') {
            $code= <<EOFCODE;
                $study;
                pos(\$subject)=0;
                \$match = ( \$subject =~ m${pat}g );
                \$got = pos(\$subject);
EOFCODE
        }
        elsif ($qr_embed) {
            $code= <<EOFCODE;
                my \$RE = qr$pat;
                $study;
                \$match = (\$subject =~ /(?:)\$RE(?:)/) while \$c--;
                \$got = "$repl";
EOFCODE
        }
        elsif ($qr_embed_thr) {
            $code= <<EOFCODE;
		# Can't run the match in a subthread, but can do this and
	 	# clone the pattern the other way.
                my \$RE = threads->new(sub {qr$pat})->join();
                $study;
                \$match = (\$subject =~ /(?:)\$RE(?:)/) while \$c--;
                \$got = "$repl";
EOFCODE
        }
        else {
            $code= <<EOFCODE;
                $study;
                \$match = (\$subject =~ $OP$pat) while \$c--;
                \$got = "$repl";
EOFCODE
        }
        #$code.=qq[\n\$expect="$expect";\n];
        #use Devel::Peek;
        #die Dump($code) if $pat=~/\\h/ and $subject=~/\x{A0}/;
	{
	    # Probably we should annotate specific tests with which warnings
	    # categories they're known to trigger, and hence should be
	    # disabled just for that test
	    no warnings qw(uninitialized regexp);
	    eval $code;
	}
	chomp( my $err = $@ );
	if ($result eq 'c') {
	    if ($err !~ m!^\Q$expect!) { print "not ok $test$todo (compile) $input => `$err'\n"; next TEST }
	    last;  # no need to study a syntax error
	}
	elsif ( $skip ) {
	    print "ok $test # skipped", length($reason) ? " $reason" : '', "\n";
	    next TEST;
	}
	elsif ( $todo_qr ) {
	    print "not ok $test # TODO", length($reason) ? " - $reason" : '', "\n";
	    next TEST;
	}
	elsif ($@) {
	    print "not ok $test$todo $input => error `$err'\n$code\n$@\n"; next TEST;
	}
	elsif ($result =~ /^n/) {
	    if ($match) { print "not ok $test$todo ($study) $input => false positive\n"; next TEST }
	}
	else {
	    if (!$match || $got ne $expect) {
	        eval { require Data::Dumper };
		if ($@) {
		    print "not ok $test$todo ($study) $input => `$got', match=$match\n$code\n";
		}
		else { # better diagnostics
		    my $s = Data::Dumper->new([$subject],['subject'])->Useqq(1)->Dump;
		    my $g = Data::Dumper->new([$got],['got'])->Useqq(1)->Dump;
		    print "not ok $test$todo ($study) $input => `$got', match=$match\n$s\n$g\n$code\n";
		}
		next TEST;
	    }
	}
    }
    print "ok $test$todo\n";
}

1;

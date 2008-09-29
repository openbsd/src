#!./perl -w

# Tests for the source filters in coderef-in-@INC

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    if ($ENV{PERL_CORE_MINITEST}) {
        print "1..0 # Skip: no dynamic loading on miniperl\n";
        exit 0;
    }
    unless (find PerlIO::Layer 'perlio') {
	print "1..0 # Skip: not perlio\n";
	exit 0;
    }
    require "test.pl";
}
use strict;
use Config;
use Filter::Util::Call;

plan(tests => 141);

unshift @INC, sub {
    no warnings 'uninitialized';
    ref $_[1] eq 'ARRAY' ? @{$_[1]} : $_[1];
};

my $fh;

open $fh, "<", \'pass("Can return file handles from \@INC");';
do $fh or die;

my @origlines = ("# This is a blank line\n",
		 "pass('Can return generators from \@INC');\n",
		 "pass('Which return multiple lines');\n",
		 "1",
		 );
my @lines = @origlines;
sub generator {
    $_ = shift @lines;
    # Return of 0 marks EOF
    return defined $_ ? 1 : 0;
};

do \&generator or die;

@lines = @origlines;
# Check that the array dereferencing works ready for the more complex tests:
do [\&generator] or die;

sub generator_with_state {
    my $param = $_[1];
    is (ref $param, 'ARRAY', "Got our parameter");
    $_ = shift @$param;
    return defined $_ ? 1 : 0;
}

do [\&generator_with_state,
    ["pass('Can return generators which take state');\n",
     "pass('And return multiple lines');\n",
    ]] or die;
   

open $fh, "<", \'fail("File handles and filters work from \@INC");';

do [$fh, sub {s/fail/pass/; return;}] or die;

open $fh, "<", \'fail("File handles and filters with state work from \@INC");';

do [$fh, sub {s/$_[1]/pass/; return;}, 'fail'] or die;

print "# 2 tests with pipes from subprocesses.\n";

my ($echo_command, $pass_arg, $fail_arg);

if ($^O eq 'VMS') {
    $echo_command = 'write sys$output';
    $pass_arg = '"pass"';
    $fail_arg = '"fail"';
}
else {
    $echo_command = 'echo';
    $pass_arg = 'pass';
    $fail_arg = 'fail';
}

open $fh, "$echo_command $pass_arg|" or die $!;

do $fh or die;

open $fh, "$echo_command $fail_arg|" or die $!;

do [$fh, sub {s/$_[1]/pass/; return;}, 'fail'] or die;

sub rot13_filter {
    filter_add(sub {
		   my $status = filter_read();
		   tr/A-Za-z/N-ZA-Mn-za-m/;
		   $status;
	       })
}

open $fh, "<", \<<'EOC';
BEGIN {rot13_filter};
cnff("This will rot13'ed prepend");
EOC

do $fh or die;

open $fh, "<", \<<'EOC';
ORTVA {ebg13_svygre};
pass("This will rot13'ed twice");
EOC

do [$fh, sub {tr/A-Za-z/N-ZA-Mn-za-m/; return;}] or die;

my $count = 32;
sub prepend_rot13_filter {
    filter_add(sub {
		   my $previous = $_;
		   # Filters should append to any existing data in $_
		   # But (logically) shouldn't filter it twice.
		   my $test = "fzrt!";
		   $_ = $test;
		   my $status = filter_read();
		   my $got = substr $_, 0, length $test, '';
		   is $got, $test, "Upstream didn't alter existing data";
		   tr/A-Za-z/N-ZA-Mn-za-m/;
		   $_ = $previous . $_;
		   die "Looping infinitely" unless $count--;
		   $status;
	       })
}

open $fh, "<", \<<'EOC';
ORTVA {cercraq_ebg13_svygre};
pass("This will rot13'ed twice");
EOC

do [$fh, sub {tr/A-Za-z/N-ZA-Mn-za-m/; return;}] or die;

# This generates a heck of a lot of oks, but I think it's necessary.
my $amount = 1;
sub prepend_block_counting_filter {
    filter_add(sub {
		   my $output = $_;
		   my $count = 256;
		   while (--$count) {
		       $_ = '';
		       my $status = filter_read($amount);
		       cmp_ok (length $_, '<=', $amount, "block mode works?");
		       $output .= $_;
		       if ($status <= 0 or /\n/s) {
			   $_ = $output;
			   return $status;
		       }
		   }
		   die "Looping infinitely";
			  
	       })
}

open $fh, "<", \<<'EOC';
BEGIN {prepend_block_counting_filter};
pass("one by one");
pass("and again");
EOC

do [$fh, sub {return;}] or die;

open $fh, "<", \<<'EOC';
BEGIN {prepend_block_counting_filter};
pas("SSS make s fast SSS");
EOC

TODO: {
    todo_skip "disabled under -Dmad", 50 if $Config{mad};
    do [$fh, sub {s/s/ss/gs; s/([\nS])/$1$1$1/gs; return;}] or die;
}

sub prepend_line_counting_filter {
    filter_add(sub {
		   my $output = $_;
		   $_ = '';
		   my $status = filter_read();
		   my $newlines = tr/\n//;
		   cmp_ok ($newlines, '<=', 1, "1 line at most?");
		   $_ = $output . $_ if defined $output;
		   return $status;
	       })
}

open $fh, "<", \<<'EOC';
BEGIN {prepend_line_counting_filter};
pass("You should see this line thrice");
EOC

do [$fh, sub {$_ .= $_ . $_; return;}] or die;

do \"pass\n(\n'Scalar references are treated as initial file contents'\n)\n"
or die;

open $fh, "<", \"ss('The file is concatentated');";

do [\'pa', $fh] or die;

open $fh, "<", \"ff('Gur svygre vf bayl eha ba gur svyr');";

do [\'pa', $fh, sub {tr/A-Za-z/N-ZA-Mn-za-m/; return;}] or die;

open $fh, "<", \"SS('State also works');";

do [\'pa', $fh, sub {s/($_[1])/lc $1/ge; return;}, "S"] or die;

@lines = ('ss', '(', "'you can use a generator'", ')');

do [\'pa', \&generator] or die;

do [\'pa', \&generator_with_state,
    ["ss('And generators which take state');\n",
     "pass('And return multiple lines');\n",
    ]] or die;

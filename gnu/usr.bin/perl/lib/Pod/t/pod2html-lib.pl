require Cwd;
require Pod::Html;
require Config;
use File::Spec::Functions;

sub convert_n_test {
    my($podfile, $testname) = @_;

    my $cwd = Cwd::cwd();
    my $base_dir = catdir $cwd, updir(), "lib", "Pod";
    my $new_dir  = catdir $base_dir, "t";
    my $infile   = catfile $new_dir, "$podfile.pod";
    my $outfile  = catfile $new_dir, "$podfile.html";

    Pod::Html::pod2html(
        "--podpath=t",
        "--podroot=$base_dir",
        "--htmlroot=/",
        "--infile=$infile",
        "--outfile=$outfile"
    );


    my ($expect, $result);
    {
	local $/;
	# expected
	$expect = <DATA>;
	$expect =~ s/\[PERLADMIN\]/$Config::Config{perladmin}/;
	if (ord("A") == 193) { # EBCDIC.
	    $expect =~ s/item_mat_3c_21_3e/item_mat_4c_5a_6e/;
	}

	# result
	open my $in, $outfile or die "cannot open $outfile: $!";
	$result = <$in>;
	close $in;
    }

    ok($expect eq $result, $testname) or do {
	my $diff = '/bin/diff';
	-x $diff or $diff = '/usr/bin/diff';
	if (-x $diff) {
	    my $expectfile = "pod2html-lib.tmp";
	    open my $tmpfile, ">", $expectfile or die $!;
	    print $tmpfile $expect;
	    close $tmpfile;
	    my $diffopt = $^O eq 'linux' ? 'u' : 'c';
	    open my $diff, "diff -$diffopt $expectfile $outfile |" or die $!;
	    print "# $_" while <$diff>;
	    close $diff;
	    unlink $expectfile;
	}
    };

    # pod2html creates these
    1 while unlink $outfile;
    1 while unlink "pod2htmd.tmp";
    1 while unlink "pod2htmi.tmp";
}

1;

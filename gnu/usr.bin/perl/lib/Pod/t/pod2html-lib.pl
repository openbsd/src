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


    local $/;
    # expected
    my $expect = <DATA>;
    $expect =~ s/\[PERLADMIN\]/$Config::Config{perladmin}/;
    if (ord("A") == 193) { # EBCDIC.
	$expect =~ s/item_mat%3c%21%3e/item_mat%4c%5a%6e/;
    }

    # result
    open my $in, $outfile or die "cannot open $outfile: $!";
    my $result = <$in>;
    close $in;
    1 while unlink $outfile;

    is($expect, $result, $testname);
    # pod2html creates these
    1 while unlink "pod2htmd.x~~";
    1 while unlink "pod2htmi.x~~";
}

1;

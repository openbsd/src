#!/usr/bin/perl -w
use strict;

=for comment

Documentation for this is in bisect-runner.pl

=cut

# The default, auto_abbrev will treat -e as an abbreviation of --end
# Which isn't what we want.
use Getopt::Long qw(:config pass_through no_auto_abbrev);

my ($start, $end, $validate, $usage, $bad);
$bad = !GetOptions('start=s' => \$start, 'end=s' => \$end,
                   validate => \$validate, 'usage|help|?' => \$usage);
unshift @ARGV, '--help' if $bad || $usage;
unshift @ARGV, '--validate' if $validate;

my $runner = $0;
$runner =~ s/bisect\.pl/bisect-runner.pl/;

die "Can't find bisect runner $runner" unless -f $runner;

system $^X, $runner, '--check-args', '--check-shebang', @ARGV and exit 255;
exit 255 if $bad;
exit 0 if $usage;

{
    my ($dev0, $ino0) = stat $0;
    die "Can't stat $0: $!" unless defined $ino0;
    my ($dev1, $ino1) = stat 'Porting/bisect.pl';
    die "Can't run a bisect using the directory containing $runner"
      if defined $dev1 && $dev0 == $dev1 && $ino0 == $ino1;
}

my $start_time = time;

# We try these in this order for the start revision if none is specified.
my @stable = qw(perl-5.005 perl-5.6.0 perl-5.8.0 v5.10.0 v5.12.0 v5.14.0);

{
    my ($dev_C, $ino_C) = stat 'Configure';
    my ($dev_c, $ino_c) = stat 'configure';
    if (defined $dev_C && defined $dev_c
        && $dev_C == $dev_c && $ino_C == $ino_c) {
        print "You seem to be on a case-insensitive file system.\n\n";
    } else {
        unshift @stable, qw(perl-5.002 perl-5.003 perl-5.004)
    }
}

$end = 'blead' unless defined $end;

# Canonicalising branches to revisions before moving the checkout permits one
# to use revisions such as 'HEAD' for --start or --end
foreach ($start, $end) {
    next unless $_;
    $_ = `git rev-parse $_`;
    die unless defined $_;
    chomp;
}

my $modified = () = `git ls-files --modified --deleted --others`;

die "This checkout is not clean - $modified modified or untracked file(s)"
    if $modified;

sub validate {
    my $commit = shift;
    if (defined $start && `git rev-list -n1 $commit ^$start^` eq "") {
        print "Skipping $commit, as it is earlier than $start\n";
        return;
    }
    if (defined $end && `git rev-list -n1 $end ^$commit^` eq "") {
        print "Skipping $commit, as it is more recent than $end\n";
        return;
    }
    print "Testing $commit...\n";
    system "git checkout $commit </dev/null" and die;
    my $ret = system $^X, $runner, '--no-clean', @ARGV;
    die "Runner returned $ret, not 0 for revision $commit" if $ret;
    system 'git clean -dxf </dev/null' and die;
    system 'git reset --hard HEAD </dev/null' and die;
    return $commit;
}

if ($validate) {
    require Text::Wrap;
    my @built = map {validate $_} 'blead', reverse @stable;
    if (@built) {
        print Text::Wrap::wrap("", "", "Successfully validated @built\n");
        exit 0;
    }
    print "Did not validate anything\n";
    exit 1;
}

my $git_version = `git --version`;
if (defined $git_version
    && $git_version =~ /\Agit version (\d+\.\d+\.\d+)(.*)/) {
    $git_version = eval "v$1";
} else {
    $git_version = v0.0.0;
}

if ($git_version ge v1.6.6) {
    system "git bisect reset HEAD" and die;
} else {
    system "git bisect reset" and die;
}

# Sanity check the first and last revisions:
system "git checkout $end" and die;
my $ret = system $^X, $runner, @ARGV;
die "Runner returned $ret for end revision" unless $ret;

if (defined $start) {
    system "git checkout $start" and die;
    my $ret = system $^X, $runner, @ARGV;
    die "Runner returned $ret, not 0 for start revision" if $ret;
} else {
    # Try to find the earliest version for which the test works
    my @tried;
    foreach my $try (@stable) {
        if (`git rev-list -n1 $end ^$try^` eq "") {
            print "Skipping $try, as it is more recent than end commit "
                . (substr $end, 0, 16) . "\n";
            # As @stable is supposed to be in age order, arguably we should
            # last; here.
            next;
        }
        system "git checkout $try" and die;
        my $ret = system $^X, $runner, @ARGV;
        if (!$ret) {
            $start = $try;
            last;
        }
        push @tried, $try;
    }
    die "Can't find a suitable start revision to default to.\nTried @tried"
        unless defined $start;
}

system "git bisect start" and die;
system "git bisect good $start" and die;
system "git bisect bad $end" and die;

# And now get git bisect to do the hard work:
system 'git', 'bisect', 'run', $^X, $runner, @ARGV and die;

END {
    my $end_time = time;

    printf "That took %d seconds\n", $end_time - $start_time
        if defined $start_time;
}

=for comment

Documentation for this is in bisect-runner.pl

=cut

# Local variables:
# cperl-indent-level: 4
# indent-tabs-mode: nil
# End:
#
# ex: set ts=8 sts=4 sw=4 et:

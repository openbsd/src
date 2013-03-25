#!/usr/bin/perl -w                                         # -*- perl -*-

BEGIN {
    die "Run me from outside the t/ directory, please" unless -d 't';
}

# test the directory cache
# XXX test --flush and %Pages being loaded/used for cross references

use strict;
use Cwd;
use Pod::Html;
use Data::Dumper;
use Test::More tests => 10;

my $cwd = Pod::Html::_unixify(Cwd::cwd());
my $infile = "t/cache.pod";
my $outfile = "cacheout.html";
my $cachefile = "pod2htmd.tmp";
my $tcachefile = "t/pod2htmd.tmp";

unlink $cachefile, $tcachefile;
is(-f $cachefile, undef, "No cache file to start");
is(-f $tcachefile, undef, "No cache file to start");

# test podpath and podroot
Pod::Html::pod2html(
    "--infile=$infile",
    "--outfile=$outfile",
    "--podpath=scooby:shaggy:fred:velma:daphne",
    "--podroot=$cwd",
    );
is(-f $cachefile, 1, "Cache created");
open(my $cache, '<', $cachefile) or die "Cannot open cache file: $!";
chomp(my $podpath = <$cache>);
chomp(my $podroot = <$cache>);
close $cache;
is($podpath, "scooby:shaggy:fred:velma:daphne", "podpath");
is($podroot, "$cwd", "podroot");

# test cache contents
Pod::Html::pod2html(
    "--infile=$infile",
    "--outfile=$outfile",
    "--cachedir=t",
    "--podpath=t",
    "--htmldir=$cwd",
    );
is(-f $tcachefile, 1, "Cache created");
open($cache, '<', $tcachefile) or die "Cannot open cache file: $!";
chomp($podpath = <$cache>);
chomp($podroot = <$cache>);
is($podpath, "t", "podpath");
my %pages;
while (<$cache>) {
    /(.*?) (.*)$/;
    $pages{$1} = $2;
}
chdir("t");
my %expected_pages = 
    # chop off the .pod and set the path
    map { my $f = substr($_, 0, -4); $f => "t/$f" }
    <*.pod>;
chdir($cwd);
is_deeply(\%pages, \%expected_pages, "cache contents");
close $cache;

1 while unlink $outfile;
1 while unlink $cachefile;
1 while unlink $tcachefile;
is(-f $cachefile, undef, "No cache file to end");
is(-f $tcachefile, undef, "No cache file to end");

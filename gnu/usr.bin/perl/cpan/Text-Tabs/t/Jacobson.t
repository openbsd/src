#!/usr/bin/perl -I.

# From: Dan Jacobson <jidanni at jidanni dot org>

use Text::Wrap qw(wrap $columns $huge $break);

print "1..1\n";

$huge='overflow';
$Text::Wrap::columns=9;
$break=".(?<=[,.])";
eval {
$a=$a=wrap('','',
"mmmm,n,ooo,ppp.qqqq.rrrrr,sssssssssssss,ttttttttt,uu,vvv wwwwwwwww####\n");
};

if ($@) {
	my $e = $@;
	$e =~ s/^/# /gm;
	print $e;
}
print $@ ? "not ok 1\n" : "ok 1\n";



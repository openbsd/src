#!./perl -w
# Test hyperlinks et al from Pod::ParseUtils

use Test::More tests => 22;

use strict;
use Pod::ParseUtils;

my @links = qw{
  name
  name/ident
  name/"sec"
  "sec"
  /"sec"
  http://www.perl.org/
  text|name
  text|name/ident
  text|name/"sec"
  text|"sec"
};

my @results = (
	       "P<name>",
	       "Q<ident> in P<name>",
	       "Q<sec> in P<name>",
	       "Q<sec>",
	       "Q<sec>",
	       "Q<http://www.perl.org/>",
	       "Q<text>",
	       "Q<text>",
	       "Q<text>",
	       "Q<text>",
	      );

is(@results, @links, 'sanity check - array lengths equal?');

for my $i( 0..@links ) {
  my $link = new Pod::Hyperlink( $links[$i] );
  is($link->markup, $results[$i], "test hyperlink $i");
}

# Now test lists
# This test needs to be better
my $list = new Pod::List( -indent => 4,
			  -start  => 52,
			  -file   => "itemtest.t",
			  -type   => "OL",
			);

ok($list);

is($list->indent, 4);
is($list->start, 52);
is($list->type, "OL");


# Pod::Cache

# also needs work

my $cache = new Pod::Cache;

# Store it in the cache
$cache->item(
	     -page => "Pod::ParseUtils",
	     -description => "A description",
	     -file => "file.t",
 );

my $item = $cache->find_page("Pod::ParseUtils");
ok($item, 'found item of this name');

is($cache->find_page("Junk"), undef, 'expect to find nothing');

my @i = $cache->item;
is($i[0], $item, 'item we found is the same one as the first in the list');

# Check the contents
is($item->page, "Pod::ParseUtils");
is($item->description, "A description");
is($item->file, "file.t");

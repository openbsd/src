
# Test hyperlinks et al from Pod::ParseUtils

BEGIN {
        chdir 't' if -d 't';
        @INC = '../lib';
        require Test; import Test;
        plan(tests => 22);
}

use strict;
use Pod::ParseUtils;

# First test the hyperlinks

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

ok(@results,@links);

for my $i( 0..@links ) {
  my $link = new Pod::Hyperlink( $links[$i] );
  ok($link->markup, $results[$i]);
}

# Now test lists
# This test needs to be better
my $list = new Pod::List( -indent => 4,
			  -start  => 52,
			  -file   => "itemtest.t",
			  -type   => "OL",
			);

ok($list);

ok($list->indent, 4);
ok($list->start, 52);
ok($list->type, "OL");


# Pod::Cache

# also needs work

my $cache = new Pod::Cache;

# Store it in the cache
$cache->item(
	     -page => "Pod::ParseUtils",
	     -description => "A description",
	     -file => "file.t",
 );

# Now look for an item of this name
my $item = $cache->find_page("Pod::ParseUtils");
ok($item);

# and a failure
ok($cache->find_page("Junk"), undef);

# Make sure that the item we found is the same one as the
# first in the list
my @i = $cache->item;
ok($i[0], $item);

# Check the contents
ok($item->page, "Pod::ParseUtils");
ok($item->description, "A description");
ok($item->file, "file.t");

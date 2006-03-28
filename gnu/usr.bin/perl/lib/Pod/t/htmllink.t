#!/usr/bin/perl -w                                         # -*- perl -*-

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
    unshift @INC, '../lib/Pod/t';
    require "pod2html-lib.pl";
}

use strict;
use Test::More tests => 1;

TODO: {
    local $TODO = "item 2 doesn't work as expected";
    convert_n_test("htmllink", "html links");
}

__DATA__
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>htmllink - Test HTML links</title>
<link rev="made" href="mailto:" />
</head>

<body style="background-color: white">

<p><a name="__index__"></a></p>
<!-- INDEX BEGIN -->

<ul>

	<li><a href="#name">NAME</a></li>
	<li><a href="#links">LINKS</a></li>
	<li><a href="#targets">TARGETS</a></li>
	<ul>

		<li><a href="#section1">section1</a></li>
		<li><a href="#section_2">section 2</a></li>
		<li><a href="#section_three">section three</a></li>
	</ul>

</ul>
<!-- INDEX END -->

<hr />
<p>
</p>
<h1><a name="name">NAME</a></h1>
<p>htmllink - Test HTML links</p>
<p>
</p>
<hr />
<h1><a name="links">LINKS</a></h1>
<p><a href="#section1">section1</a></p>
<p><a href="#section_2">section 2</a></p>
<p><a href="#section_three">section three</a></p>
<p><a href="#item_item1">item1</a></p>
<p><a href="#item_item_2">item 2</a></p>
<p><a href="#item_item_three">item three</a></p>
<p><a href="#section1">section1</a></p>
<p><a href="#section_2">section 2</a></p>
<p><a href="#section_three">section three</a></p>
<p><a href="#item_item1">item1</a></p>
<p><a href="#item_item_2">item 2</a></p>
<p><a href="#item_item_three">item three</a></p>
<p><a href="#section1">section1</a></p>
<p><a href="#section_2">section 2</a></p>
<p><a href="#section_three">section three</a></p>
<p><a href="#item_item1">item1</a></p>
<p><a href="#item_item_2">item 2</a></p>
<p><a href="#item_item_three">item three</a></p>
<p><a href="#section1">text</a></p>
<p><a href="#section_2">text</a></p>
<p><a href="#section_three">text</a></p>
<p><a href="#item_item1">text</a></p>
<p><a href="#item_item_2">text</a></p>
<p><a href="#item_item_three">text</a></p>
<p><a href="#section1">text</a></p>
<p><a href="#section_2">text</a></p>
<p><a href="#section_three">text</a></p>
<p><a href="#item_item1">text</a></p>
<p><a href="#item_item_2">text</a></p>
<p><a href="#item_item_three">text</a></p>
<p><a href="#section1">text</a></p>
<p><a href="#section_2">text</a></p>
<p><a href="#section_three">text</a></p>
<p><a href="#item_item1">text</a></p>
<p><a href="#item_item_2">text</a></p>
<p><a href="#item_item_three">text</a></p>
<p>
</p>
<hr />
<h1><a name="targets">TARGETS</a></h1>
<p>
</p>
<h2><a name="section1">section1</a></h2>
<p>This is section one.</p>
<p>
</p>
<h2><a name="section_2">section 2</a></h2>
<p>This is section two.</p>
<p>
</p>
<h2><a name="section_three">section three</a></h2>
<p>This is section three.</p>
<dl>
<dt><strong><a name="item_item1">item1</a></strong><br />
</dt>
<dd>
This is item one.
</dd>
<p></p>
<dt><strong><a name="item_item_2">item 2</a></strong><br />
</dt>
<dd>
This is item two.
</dd>
<p></p>
<dt><strong><a name="item_item_three">item three</a></strong><br />
</dt>
<dd>
This is item three.
</dd>
<p></p></dl>

</body>

</html>

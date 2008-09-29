#!/usr/bin/perl -w                                         # -*- perl -*-

BEGIN {
   chdir 't' if -d 't';
   unshift @INC, '../lib';
   unshift @INC, '../lib/Pod/t';
   require "pod2html-lib.pl";
}

use strict;
use Test::More tests => 1;

convert_n_test("htmlescp", "html escape");

__DATA__
<?xml version="1.0" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>NAME</title>
<meta http-equiv="content-type" content="text/html; charset=utf-8" />
<link rev="made" href="mailto:[PERLADMIN]" />
</head>

<body style="background-color: white">


<!-- INDEX BEGIN -->
<div name="index">
<p><a name="__index__"></a></p>

<ul>

	<li><a href="#name">NAME</a></li>
	<li><a href="#description">DESCRIPTION</a></li>
</ul>

<hr name="index" />
</div>
<!-- INDEX END -->

<p>
</p>
<h1><a name="name">NAME</a></h1>
<p>Escape Sequences Test</p>
<p>
</p>
<hr />
<h1><a name="description">DESCRIPTION</a></h1>
<p>I am a stupid fool who puts naked &lt; &amp; &gt; characters in my POD
instead of escaping them as &lt; and &gt;.</p>
<p>Here is some <strong>bold</strong> text, some <em>italic</em> plus <em class="file">/etc/fstab</em>
file and something that looks like an &lt;html&gt; tag.
This is some <code>$code($arg1)</code>.</p>

</body>

</html>

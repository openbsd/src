#!/usr/bin/perl -w                                         # -*- perl -*-

BEGIN {
    require "./t/pod2html-lib.pl";
}

use strict;
use Cwd;
use Test::More tests => 3;

my $cwd = cwd();
my $data_pos = tell DATA; # to read <DATA> twice

convert_n_test("htmldir2", "test --htmldir and --htmlroot 2a", 
 "--podpath=t",
 "--htmldir=t",
 "--quiet",
);

seek DATA, $data_pos, 0; # to read <DATA> twice (expected output is the same)

convert_n_test("htmldir2", "test --htmldir and --htmlroot 2b", 
 "--podpath=t",
 "--quiet",
);

seek DATA, $data_pos, 0; # to read <DATA> thrice (expected output is the same)

# this test makes sure paths are absolute unless --htmldir is specified
convert_n_test("htmldir2", "test --htmldir and --htmlroot 2c", 
 "--podpath=t",
 "--podroot=$cwd",
 "--norecurse", # testing --norecurse, too
 "--quiet",
);

__DATA__
<?xml version="1.0" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>htmldir - Test --htmldir feature</title>
<meta http-equiv="content-type" content="text/html; charset=utf-8" />
<link rev="made" href="mailto:[PERLADMIN]" />
</head>

<body>



<ul id="index">
  <li><a href="#NAME">NAME</a></li>
  <li><a href="#LINKS">LINKS</a></li>
</ul>

<h1 id="NAME">NAME</h1>

<p>htmldir - Test --htmldir feature</p>

<h1 id="LINKS">LINKS</h1>

<p>Normal text, a <a>link</a> to nowhere,</p>

<p>a link to <a>perlvar-copy</a>,</p>

<p><a href="/t/htmlescp.html">htmlescp</a>,</p>

<p><a href="/t/feature.html#Another-Head-1">&quot;Another Head 1&quot; in feature</a>,</p>

<p>and another <a href="/t/feature.html#Another-Head-1">&quot;Another Head 1&quot; in feature</a>.</p>


</body>

</html>



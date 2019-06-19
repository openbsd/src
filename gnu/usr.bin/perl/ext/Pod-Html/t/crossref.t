#!/usr/bin/perl -w                                         # -*- perl -*-

BEGIN {
    require "./t/pod2html-lib.pl";
}

END {
    rem_test_dir();
}

use strict;
use Cwd;
use File::Spec::Functions;
use Test::More tests => 1;

SKIP: {
    my $output = make_test_dir();
    skip "$output", 1 if $output;
    
    my ($v, $d) = splitpath(cwd(), 1);
    my @dirs = splitdir($d);
    shift @dirs if $dirs[0] eq '';
    my $relcwd = join '/', @dirs;
        
    convert_n_test("crossref", "cross references", 
     "--podpath=". File::Spec::Unix->catdir($relcwd, 't') . ":"
                 . File::Spec::Unix->catdir($relcwd, 'testdir/test.lib'),
     "--podroot=". catpath($v, '/', ''),
     "--quiet",
    );
}

__DATA__
<?xml version="1.0" ?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<title>htmlcrossref - Test HTML cross reference links</title>
<meta http-equiv="content-type" content="text/html; charset=utf-8" />
<link rev="made" href="mailto:[PERLADMIN]" />
</head>

<body>



<ul id="index">
  <li><a href="#NAME">NAME</a></li>
  <li><a href="#LINKS">LINKS</a></li>
  <li><a href="#TARGETS">TARGETS</a>
    <ul>
      <li><a href="#section1">section1</a></li>
    </ul>
  </li>
</ul>

<h1 id="NAME">NAME</h1>

<p>htmlcrossref - Test HTML cross reference links</p>

<h1 id="LINKS">LINKS</h1>

<p><a href="#section1">&quot;section1&quot;</a></p>

<p><a href="/[RELCURRENTWORKINGDIRECTORY]/t/htmllink.html#section-2">&quot;section 2&quot; in htmllink</a></p>

<p><a href="#item1">&quot;item1&quot;</a></p>

<p><a href="#non-existent-section">&quot;non existent section&quot;</a></p>

<p><a href="/[RELCURRENTWORKINGDIRECTORY]/testdir/test.lib/var-copy.html">var-copy</a></p>

<p><a href="/[RELCURRENTWORKINGDIRECTORY]/testdir/test.lib/var-copy.html#pod">&quot;$&quot;&quot; in var-copy</a></p>

<p><code>var-copy</code></p>

<p><code>var-copy/$&quot;</code></p>

<p><a href="/[RELCURRENTWORKINGDIRECTORY]/testdir/test.lib/podspec-copy.html#First">&quot;First:&quot; in podspec-copy</a></p>

<p><code>podspec-copy/First:</code></p>

<p><a>notperldoc</a></p>

<h1 id="TARGETS">TARGETS</h1>

<h2 id="section1">section1</h2>

<p>This is section one.</p>

<dl>

<dt id="item1">item1  </dt>
<dd>

<p>This is item one.</p>

</dd>
</dl>


</body>

</html>



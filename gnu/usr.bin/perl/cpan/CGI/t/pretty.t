#!/bin/perl -w

use strict;
use Test::More tests => 17;
use CGI::Pretty ':all';

is(h1(), '<h1 />
',"single tag");

is(ol(li('fred'),li('ethel')), <<HTML,   "basic indentation");
<ol>
	<li>
		fred
	</li>
	<li>
		ethel
	</li>
</ol>
HTML


is(p('hi',pre('there'),'frog'), <<HTML, "<pre> tags");
<p>
	hi <pre>there</pre> frog
</p>
HTML

is(h1({-align=>'CENTER'},'fred'), <<HTML, "open/close tag with attribute");
<h1 align="CENTER">
	fred
</h1>
HTML

is(h1({-align=>undef},'fred'), <<HTML,"open/close tag with orphan attribute");
<h1 align>
	fred
</h1>
HTML

is(h1({-align=>'CENTER'},['fred','agnes']), <<HTML, "distributive tag with attribute");
<h1 align="CENTER">
	fred
</h1>
<h1 align="CENTER">
	agnes
</h1>
HTML

is(p('hi',a({-href=>'frog'},'there'),'frog'), <<HTML,   "as-is");
<p>
	hi <a href="frog">there</a> frog
</p>
HTML

is(p([ qw( hi there frog ) ] ), <<HTML,   "array-reference");
<p>
	hi
</p>
<p>
	there
</p>
<p>
	frog
</p>
HTML

is(p(p(p('hi'), 'there' ), 'frog'), <<HTML,   "nested tags");
<p>
	<p>
		<p>
			hi
		</p>
		there
	</p>
	frog
</p>
HTML

is(table(TR(td(table(TR(td('hi', 'there', 'frog')))))), <<HTML,   "nested as-is tags");
<table>
	<tr>
		<td><table>
			<tr>
				<td>hi there frog</td>
			</tr>
		</table></td>
	</tr>
</table>
HTML

is(table(TR(td(table(TR(td( [ qw( hi there frog ) ])))))), <<HTML,   "nested as-is array-reference");
<table>
	<tr>
		<td><table>
			<tr>
				<td>hi</td><td>there</td><td>frog</td>
			</tr>
		</table></td>
	</tr>
</table>
HTML

$CGI::Pretty::INDENT = $CGI::Pretty::LINEBREAK = ""; 

is(h1(), '<h1 />',"single tag (pretty turned off)");
is(h1('fred'), '<h1>fred</h1>',"open/close tag (pretty turned off)");
is(h1('fred','agnes','maura'), '<h1>fred agnes maura</h1>',"open/close tag multiple (pretty turned off)");
is(h1({-align=>'CENTER'},'fred'), '<h1 align="CENTER">fred</h1>',"open/close tag with attribute (pretty turned off)");
is(h1({-align=>undef},'fred'), '<h1 align>fred</h1>',"open/close tag with orphan attribute (pretty turned off)");
is(h1({-align=>'CENTER'},['fred','agnes']), '<h1 align="CENTER">fred</h1> <h1 align="CENTER">agnes</h1>',
   "distributive tag with attribute (pretty turned off)");


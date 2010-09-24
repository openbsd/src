#!/usr/local/bin/perl -w

use lib qw(t/lib);
use strict;

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(blib/lib blib/arch);
use Test::More tests => 45;

use CGI qw(:standard *h1 *h2 *h3 *h4 *h5 *h6 *table *ul *li *ol *td *b *i *u *div);

is(start_h1(), "<h1>", "start_h1"); # TEST
is(start_h1({class => 'hello'}), "<h1 class=\"hello\">", "start_h1 with param"); # TEST
is(end_h1(), "</h1>", "end_h1"); # TEST

is(start_h2(), "<h2>", "start_h2"); # TEST
is(start_h2({class => 'hello'}), "<h2 class=\"hello\">", "start_h2 with param"); # TEST
is(end_h2(), "</h2>", "end_h2"); # TEST

is(start_h3(), "<h3>", "start_h3"); # TEST
is(start_h3({class => 'hello'}), "<h3 class=\"hello\">", "start_h3 with param"); # TEST
is(end_h3(), "</h3>", "end_h3"); # TEST

is(start_h4(), "<h4>", "start_h4"); # TEST
is(start_h4({class => 'hello'}), "<h4 class=\"hello\">", "start_h4 with param"); # TEST
is(end_h4(), "</h4>", "end_h4"); # TEST

is(start_h5(), "<h5>", "start_h5"); # TEST
is(start_h5({class => 'hello'}), "<h5 class=\"hello\">", "start_h5 with param"); # TEST
is(end_h5(), "</h5>", "end_h5"); # TEST

is(start_h6(), "<h6>", "start_h6"); # TEST
is(start_h6({class => 'hello'}), "<h6 class=\"hello\">", "start_h6 with param"); # TEST
is(end_h6(), "</h6>", "end_h6"); # TEST

is(start_table(), "<table>", "start_table"); # TEST
is(start_table({class => 'hello'}), "<table class=\"hello\">", "start_table with param"); # TEST
is(end_table(), "</table>", "end_table"); # TEST

is(start_ul(), "<ul>", "start_ul"); # TEST
is(start_ul({class => 'hello'}), "<ul class=\"hello\">", "start_ul with param"); # TEST
is(end_ul(), "</ul>", "end_ul"); # TEST

is(start_li(), "<li>", "start_li"); # TEST
is(start_li({class => 'hello'}), "<li class=\"hello\">", "start_li with param"); # TEST
is(end_li(), "</li>", "end_li"); # TEST

is(start_ol(), "<ol>", "start_ol"); # TEST
is(start_ol({class => 'hello'}), "<ol class=\"hello\">", "start_ol with param"); # TEST
is(end_ol(), "</ol>", "end_ol"); # TEST

is(start_td(), "<td>", "start_td"); # TEST
is(start_td({class => 'hello'}), "<td class=\"hello\">", "start_td with param"); # TEST
is(end_td(), "</td>", "end_td"); # TEST

is(start_b(), "<b>", "start_b"); # TEST
is(start_b({class => 'hello'}), "<b class=\"hello\">", "start_b with param"); # TEST
is(end_b(), "</b>", "end_b"); # TEST

is(start_i(), "<i>", "start_i"); # TEST
is(start_i({class => 'hello'}), "<i class=\"hello\">", "start_i with param"); # TEST
is(end_i(), "</i>", "end_i"); # TEST

is(start_u(), "<u>", "start_u"); # TEST
is(start_u({class => 'hello'}), "<u class=\"hello\">", "start_u with param"); # TEST
is(end_u(), "</u>", "end_u"); # TEST

is(start_div(), "<div>", "start_div"); # TEST
is(start_div({class => 'hello'}), "<div class=\"hello\">", "start_div with param"); # TEST
is(end_div(), "</div>", "end_div"); # TEST


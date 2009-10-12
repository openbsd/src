#!/usr/local/bin/perl -w
#Author: Dan Dascalescu
use strict;
use Test::More tests => 1;

use Text::Wrap;

local $Text::Wrap::columns = 15;
local $Text::Wrap::separator2 = '[N]';

is(wrap('','','some long text here that should be wrapped on at least three lines'),
"some long text[N]here that[N]should be[N]wrapped on at[N]least three[N]lines",
'If you just to preserve existing newlines but add new breaks with something else, set $Text::Wrap::separator2 instead.');

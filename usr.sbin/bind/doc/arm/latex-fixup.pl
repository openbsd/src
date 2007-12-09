#!/usr/bin/perl -w
#
# Copyright (C) 2005  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $ISC: latex-fixup.pl,v 1.2.2.2 2005/07/19 05:55:47 marka Exp $

# Sadly, the final stages of generating a presentable PDF file always
# seem to require some manual tweaking.  Doesn't seem to matter what
# typesetting tool one uses, sane forms of automation only go so far,
# at least with present technology.
#
# This script is intended to be a collection of tweaks.  The theory is
# that, while we can't avoid the need for tweaking, we can at least
# write the silly things down in a form that a program might be able
# to execute.  Undoubtedly everythig in here will break, eventually,
# at which point it will need to be updated, but since the alternative
# is to do the final editing by hand every time, this approach seems
# the lesser of two evils.

while (<>) {

    # Fix a db2latex oops.  LaTeX2e does not like having tables with
    # duplicate names.  Perhaps the dblatex project will fix this
    # someday, but we can get by with just deleting the offending
    # LaTeX commands for now.

    s/\\addtocounter\{table\}\{-1\}//g;

    # Line break in the middle of quoting one period looks weird.

    s/{\\texttt{{\.\\dbz{}}}}/\\mbox{{\\texttt{{\.\\dbz{}}}}}/;

    # Add any further tweaking here.

    # Write out whatever we have now.
    print;
}

# This file fills in a config_h.SH template based on the data
# of the file config.def and outputs a config.h.
#
# Written January 24, 2000 by Jarkko Hietaniemi [jhi@iki.fi]
# Modified February 2, 2000 by Paul Green [Paul_Green@stratus.com]
# Modified October 23, 2000 by Paul Green [Paul_Green@stratus.com]

#
# Read in the definitions file
#

if (open(CONFIG_DEF, "config.def")) {
    while (<CONFIG_DEF>) {
        if (/^([^=]+)='(.*)'$/) {
            my ($var, $val) = ($1, $2);
            $define{$var} = $val;
            $used{$var} = 0;
        } else {
            warn "config.def: $.: illegal line: $_";
        }
    }
} else {
    die "$0: Cannot open config.def: $!";
}

close (CONFIG_DEF);

#
# Open the template input file.
#

$lineno = 0;
unless (open(CONFIG_SH, "../config_h.SH")) {
    die "$0: Cannot open ../config_h.SH: $!";
}

#
# Open the output file.
#

unless (open(CONFIG_H, ">config.h.new")) {
    die "$0: Cannot open config.h.new for output: $!";
}

#
#   Skip lines before the first !GROK!THIS!
#

while (<CONFIG_SH>) {
    $lineno = $lineno + 1;
    last if /^sed <<!GROK!THIS!/;
}

#
#   Process the rest of the file, a line at a time.
#   Stop when the next !GROK!THIS! is found.
#

while (<CONFIG_SH>) {
    $lineno = $lineno + 1;
    last if /^!GROK!THIS!/;
#
#   The definition of SITEARCH and SITEARCH_EXP has to be commented-out.
#   The easiest way to do this is to special-case it here.
#
    if (/^#define SITEARCH*/) {
        s@(^.*$)@/*$1@;
    }
#
#   The case of #$d_foo at the BOL has to be handled carefully.
#   If $d_foo is "undef", then we must first comment out the entire line.
#
    if (/^#(\$\w+)/) {
        if (exists $define{$1}) {
            $used{$1}=1;
            s@^#(\$\w+)@("$define{$1}" eq "undef") ?
                "/*#define":"#$define{$1}"@e;
        }
    }
#
#   There could be multiple $variables on this line.
#   Find and replace all of them.
#
    if (/(\$\w+)/) {
        s/(\$\w+)/(exists $define{$1}) ?
            (($used{$1}=1),$define{$1}) :
            ((print "Undefined keyword $1 on line $lineno\n"),$1)/ge;
        print CONFIG_H;
    }
#
#   There are no variables, just print the line out.
#
    else {
        print CONFIG_H;
    }
}

unless (close (CONFIG_H)) {
    die "$0: Cannot close config.h.new: $!";
    }

close (CONFIG_SH);

while (($key,$value) = each %used) {
    if ($value == 0) {
        print "Unused keyword definition: $key\n";
    }
}


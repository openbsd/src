# This file fills in a config_h.SH template based on the data
# of the file config.def and outputs a config.h.
#
# Written January 24, 2000 by Jarkko Hietaniemi [jhi@iki.fi]
# Modified February 2, 2000 by Paul Green [Paul_Green@stratus.com]

#
# Read in the definitions file
#

if (open(CONFIG_DEF, "config.def")) {
    while (<CONFIG_DEF>) {
        if (/^([^=]+)='(.*)'$/) {
            my ($var, $val) = ($1, $2);
            $define{$var} = $val;
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

unless (open(CONFIG_SH, "config_h.SH_orig")) {
    die "$0: Cannot open config_h.SH_orig: $!";
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
    last if /^sed <<!GROK!THIS!/;
}

#
#   Process the rest of the file, a line at a time.
#   Stop when the next !GROK!THIS! is found.
#

while (<CONFIG_SH>) {
    last if /^!GROK!THIS!/;
#
#   The case of #$d_foo at the BOL has to be handled carefully.
#   If $d_foo is "undef", then we must first comment out the entire line.
#
    if (/^#\$\w+/) {
        s@^#(\$\w+)@("$define{$1}" eq "undef")?"/*#define":"#$define{$1}"@e;
    }
#
#   There could be multiple $variables on this line.
#   Find and replace all of them.
#
    if (/(\$\w+)/) {
        s/(\$\w+)/(exists $define{$1}) ? $define{$1} : $1/ge;
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

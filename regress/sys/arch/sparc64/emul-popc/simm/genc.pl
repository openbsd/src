#!/bin/sh
#	$OpenBSD: genc.pl,v 1.2 2003/07/10 14:42:36 jason Exp $
#
# Copyright (c) 2003 Jason L. Wright (jason@thought.net)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

$lo = -4096;
$hi = 4095;

print "/* AUTOMATICALLY GENERATED, DO NOT EDIT */\n";
print "#include <sys/types.h>\n";
print "#include <stdio.h>\n";
print "\n";

for ($i = $lo; $i <= $hi; $i++) {
	print "extern int64_t popc";
	if ($i < 0) {
		$v =  -$i;
		print "__$v";
	} else {
		print "_$i";
	}
	print "(void);\n";
}

print "\n";

print "int main(int argc, char *argv[]) {\n";
for ($i = $lo; $i <= $hi; $i++) {
	print "\tprintf(\"$i: %qd\\n\", popc";
	if ($i < 0) {
		$v =  -$i;
		print "__$v";
	} else {
		print "_$i";
	}
	print "());\n";
}
print "}\n";


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

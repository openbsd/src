hack : module

dcl i int;

fred : proc (a int in, b int loc);
  writetext(stdout, "a was '%C'; b was '%C'.%/", a, b);
  b + := 1;
end fred;

klaus : proc ();
  writetext(stdout, "here's klaus calling.%/");
end klaus;

i:=12;
writetext(stdout, "done.%/");

end hack;

x: module                        -- line 1
  p:proc (t char (20) varying);  --      2
	  writetext(stdout, t);  --      3
  end p;                         --      4
                                 --      5
  p("Jason Dark.%/");            --      6
  p("Hello World.%/");           --      7
end x;        

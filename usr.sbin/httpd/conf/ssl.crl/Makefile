##
##  Makefile to keep the hash symlinks in SSLCARevocationPath up to date
##  Copyright (c) 1998-2000 Ralf S. Engelschall, All Rights Reserved. 
##

SSL_PROGRAM=

update: clean
	-@ssl_program="$(SSL_PROGRAM)"; \
	if [ ".$$ssl_program" = . ]; then \
	    for dir in . `echo $$PATH | sed -e 's/:/ /g'`; do \
	        for program in openssl ssleay; do \
	            if [ -f "$$dir/$$program" ]; then \
	                if [ -x "$$dir/$$program" ]; then \
	                    ssl_program="$$dir/$$program"; \
						break; \
	                fi; \
	            fi; \
	        done; \
	        if [ ".$$ssl_program" != . ]; then \
				break; \
	        fi; \
	    done; \
	fi; \
	if [ ".$$ssl_program" = . ]; then \
	    echo "Error: neither 'openssl' nor 'ssleay' program found" 1>&2; \
	    exit 1; \
	fi; \
	for file in *.crl; do \
	    [ "x$$file" = "x*.crl" ] && continue; \
	    if [ ".`grep SKIPME $$file`" != . ]; then \
	        echo dummy |\
	        awk '{ printf("%-15s ... Skipped\n", file); }' \
	        "file=$$file"; \
	    else \
	        n=0; \
	        while [ 1 ]; do \
	            hash="`$$ssl_program crl -noout -hash <$$file`"; \
	            if [ -r "$$hash.$$n" ]; then \
	                n=`expr $$n + 1`; \
	            else \
	                echo dummy |\
	                awk '{ printf("%-15s ... %s\n", file, hash); }' \
	                "file=$$file" "hash=$$hash.r$$n"; \
	                ln -s $$file $$hash.r$$n; \
	                break; \
	            fi; \
	        done; \
	    fi; \
	done

clean:
	-@rm -f [0-9a-fA-F]*.r[0-9]*


LESSONS = files editor morefiles macros eqn C

CFLAGS = -g
LLIB	= ../lib

cp:	all
	cp learn tee lcount $(LLIB)
	@echo "Do 'make lessons' if you need to extract the lesson archives"
	@echo "Do 'make play; make log' to make playpen and log directories"

all:	learn tee lcount

learn: learn.c learn.h
	cc -o learn $(CFLAGS) '-DLLIB="$(LLIB)"' learn.c

lcount tee:
	cc $(CFLAGS) $@.c -o $@

lessons:	$(LESSONS)

$(LESSONS):
	-rm -rf $(LLIB)/$@
	mkdir $(LLIB)/$@
	(cd $(LLIB)/$@; ar x ../$@.a)

play log:
	-rm -rf $(LLIB)/$@; mkdir $(LLIB)/$@; chmod +w $(LLIB)/$@

check:
	-@test -r $(LLIB)/tee || echo 'tee not present; make tee'
	-@test -r $(LLIB)/lcount || echo 'lcount not present; make lcount'
	-@test -r $(LLIB)/play || echo 'play directory not present; make play'
	-@test -r $(LLIB)/log || echo 'log directory not present; make log'
	-@for i in $(LESSONS); do test -r $(LLIB)/$$i/L0 || echo $$i not unarchived, make $$i; done

clean:
	rm -rf lcount learn tee $(LLIB)/play $(LLIB)/log

targets		?= ex1 ex2
target_fails	?= ex-fail

PRELINK		= ../test-double-elfpatch -w -s .stub

default: all

all:
	@for x in $(targets); do \
	    echo "*** Building $$x ***" ;\
	    $(MAKE) --no-print-directory clean ;\
	    $(MAKE) --no-print-directory $$x ;\
	    echo "*** Executing $$x " ;\
	    test -x $$x && ./$$x && echo OK || echo FAIL;\
	done

ex1: main.o func.o
	$(LINK.c) -o $@ $^

ex2: main.o func.o stub.o
	$(PRELINK) $^
	$(LINK.c) -o $@ $^

ex-fail: main.o func.o stub.o
	$(LINK.c) -o $@ $^

clean:
	rm -f *.o $(targets)

clean-all: clean
	rm -f $(target_fails) *~


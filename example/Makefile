targets		?= test1 test2 test3

PRELINK		= ../test-double-elfpatch -w -s .stub

default: all

all:
	@for x in $(targets); do \
	    echo "*** TEST $$x ***" ;\
	    $(MAKE) --no-print-directory clean ;\
	    $(MAKE) --no-print-directory $$x ;\
	    echo "*** Executing $$x " ;\
	    test -x $$x && ./$$x && echo PASS || echo FAIL;\
	done

clean:
	rm -f *.o $(targets)

test1: main.o func.o
	$(LINK.c) -o $@ $^

test2: main.o func.o test-double-func.o
	$(LINK.c) -o $@ $^

test3: main.o func.o test-double-func.o
	$(PRELINK) $^
	$(LINK.c) -o $@ $^

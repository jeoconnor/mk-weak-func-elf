targets		= test-link test-single-func test-multi-func test-section-func

failures	= test-single-func-fail

SECTION		= .mock

CPPFLAGS	= -DMOCK_LABEL=$(SECTION)

all:
	for x in $(targets); do \
	    $(MAKE) clean ;\
	    $(MAKE) $$x ;\
	    echo "executing $$x" ; ./$$x && echo PASS || echo FAIL ;\
	done

test-link: test-single-func.o func.o
	$(LINK.c) $^ -o $@


test-single-func: test-single-func.o mock-func.o func.o
	../mock-elfpatch -W -m mock-func.o func.o
	$(LINK.c) $^ -o $@

test-multi-func: test-multi-func.o mock-func.o func.o
	../mock-elfpatch -W -m mock-func.o func.o
	$(LINK.c) $^ -o $@

test-section-func: LDFLAGS=-Xlinker --unique=$(SECTION)
test-section-func: test-multi-func.o func-section.o func.o
	../mock-elfpatch -W $^
	$(LINK.c) $^ -o $@

clean:
	rm -f $(targets) *.o

test-single-func-fail: test-single-func.o mock-func.o func.o
	$(LINK.c) $^ -o $@


targets	= test-link test-single-func test-multi-func

all:
	for x in $(targets); do \
	    $(MAKE) clean ;\
	    $(MAKE) $$x ;\
	    echo "executing $$x" ; ./$$x ;\
	done

test-link: test-single-func.o func.o
	$(LINK.c) $^ -o $@


test-single-func: test-single-func.o mock-func.o func.o
	../mock-elfpatch -W -m mock-func.o func.o
	$(LINK.c) $^ -o $@

test-multi-func: test-multi-func.o mock-func.o func.o
	../mock-elfpatch -W -m mock-func.o func.o
	$(LINK.c) $^ -o $@

clean:
	rm -f $(targets) *.o


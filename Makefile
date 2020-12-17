target		= elfwalk
source		= elfwalk.cpp
object		= $(source:.cpp=.o)

LD		= g++
CPPFLAGS	= -ggdb -Wall -std=gnu++17
LOADLIBES	= -lelf

default: $(target)

$(target): $(object)
	$(LINK.cpp) $(LOADLIBES) $^ -o $@

clean:
	rm -f $(object) *~ $(target)

CFLAGS += -O2 -g -Wall `pkg-config --cflags libpcsclite`

# -ldl to use ldopen(), etc.
# -rdynamic so that the driver can use debug_msg()

# GNU/Linux
LDFLAGS = -ldl -rdynamic
# *BSD
#LDFLAGS = -rdynamic
# Mac OS X
#CFLAGS = -I. -DRESPONSECODE_DEFINED_IN_WINTYPES_H -DMAX_BUFFER_SIZE_EXTENDED="(4 + 3 + (1<<16) + 3)" -DHAVE_STRLCPY -DHAVE_STRLCAT -I../../PCSC/src/PCSC
#LDFLAGS = -framework PCSC

targets = handler_test.o debug.o tokenparser.o strlcpy.o simclist.o

all: handler_test

handler_test: $(targets)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f .dependencies
	touch .dependencies
	rm -f handler_test $(targets)

.dependencies:
	touch .dependencies

distclean: clean
	rm -f dep_stamp
	rm -f tags

ctags:
	ctags-exuberant *.c *.h

dep: dep_stamp
dep_stamp:
	@echo "Making dependencies..."
	makedepend -f - -I. *.c > .dependencies
	touch dep_stamp

.PHONY: all clean distclean ctags dep

include .dependencies


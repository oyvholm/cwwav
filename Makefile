CC=gcc
CFLAGS=-Wall -Wextra
LDLIBS=-lm -lsndfile

ifeq ($(LAME),1)
  LDFLAGS+=-lmp3lame
  CFLAGS+=-DHAVE_LAME=1
endif

ifeq ($(DEBUG),1)
  LDFLAGS+=-ggdb
  CFLAGS+=-ggdb
endif

all: cwwav

clean:
	rm -f cwwav

install: all
	install -d /usr/local/bin
	install -m 0755 cwwav /usr/local/bin/cwwav

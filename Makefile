CC=gcc
CFLAGS=-ggdb
LDFLAGS=-lm -lsndfile -lmp3lame -ggdb

all: cwwav

clean:
	rm cwwav

install: all
	install -d /usr/local/bin
	install -m 0755 cwwav /usr/local/bin/cwwav

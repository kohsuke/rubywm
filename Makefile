
CC = gcc

OPTS = -fPIC -Wall -g -02

INC = \
		-I. \
		-I/usr/X11R6/include \
		-I/usr/include/glib-2.0 \
		-I/usr/lib/glib-2.0/include \
		-I/usr/lib/ruby/1.8/i386-linux \
		-I/usr/lib/ruby/1.8/i386-linux

LIB = \
		-L/usr/lib \
		-L/usr/X11R6/lib \
		-lSM -lICE -lX11 -lXmu -lglib-2.0 \
		-lruby1.8 -lpthread -ldl -lcrypt -lm -lc

all:
	$(CC) $(INC) -c wmlib.c
	$(CC) -shared -o wmlib.so wmlib.o $(LIB)

clean:
	rm -f *.o *.so

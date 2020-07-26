
CC = gcc

OPTS = -fPIC -Wall -g

INC = \
		-I. \
		-I/usr/X11R6/include \
		-I/usr/include/glib-2.0 \
		-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
		-I/usr/include/x86_64-linux-gnu/ruby-2.5.0 \
		-I/usr/include/ruby-2.5.0

LIB = \
		-L/usr/lib \
		-L/usr/X11R6/lib \
		-lSM -lICE -lX11 -lXmu -lglib-2.0 \
		-lruby-2.5 -lpthread -ldl -lcrypt -lm -lc

all:
	$(CC) $(OPTS) $(INC) -c wmlib.c
	$(CC) $(OPTS) -shared -o wmlib.so wmlib.o $(LIB)

clean:
	rm -f *.o *.so

CC = gcc
CFLAGS = -O2 
CFLAGS += -g
LDFLAGS = -pthread

OBJS = proxy.o csapp.o
#display.o

TEAM = NOBODY
VERSION = 1

all: proxy

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c
	$(CC) $(CFLAGS) -c proxy.c

#display.o: display.c
#	$(CC) $(CFLAGS) -I/usr/X11R6/include  -c display.c

proxy: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o proxy
#handin:
#	cp proxy.c $(INDIR)/$(TEAM)-$(VERSION)-proxy.c
#	cp csapp.c $(INDIR)/$(TEAM)-$(VERSION)-csapp.c
#	cp csapp.h $(INDIR)/$(TEAM)-$(VERSION)-csapp.h

clean:
	rm -f *~ csapp.o proxy proxy.o display.o core

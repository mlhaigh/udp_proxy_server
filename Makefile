CC=gcc
CFLAGS=-g -Wall

#default: UDPProxy
default: UDPClient

INCS= -I/usr/include/glib-2.0

clean:
	rm -f *.o UDPProxy UDPClient UDPServer

UDPClient: UDPClient.c UDPProxy.h UDPServer
	$(CC) -o UDPClient UDPClient.c $(INCS) `pkg-config --cflags --libs glib-2.0`

UDPServer: UDPServer.c UDPProxy.h
	$(CC) -o UDPServer UDPServer.c $(INCS) `pkg-config --cflags --libs glib-2.0`

#UDPProxy: UDPClient UDPServer UDPProxy.c UDPProxy.h
#	$(CC) -o UDPProxy UDPProxy.c $(INCS) `pkg-config --cflags --libs glib-2.0`

all: clean default


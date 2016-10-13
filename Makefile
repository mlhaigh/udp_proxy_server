CC=gcc
CFLAGS=-g -Wall

#default: UDPProxy
default: UDPClient

clean:
	rm -f *.o UDPProxy UDPClient UDPServer hashtable

UDPProxy: UDPProxy.h UDPServer UDPClient hashtable.c
	$(CC) -o UDPProxy UDPProxy.c hashtable.c

UDPClient: UDPClient.c UDPProxy.h UDPServer
	$(CC) -o UDPClient UDPClient.c 

UDPServer: UDPServer.c UDPProxy.h
	$(CC) -o UDPServer UDPServer.c 

hashtable:
	$(CC) -o hashtable hashtable.c

#UDPProxy: UDPClient UDPServer UDPProxy.c UDPProxy.h
#	$(CC) -o UDPProxy UDPProxy.c $(INCS) `pkg-config --cflags --libs glib-2.0`

all: clean default


CC=gcc
CFLAGS=-g -Wall

default: UDPProxy

clean:
	rm -f *.o UDPProxy UDPClient UDPServer

UDPClient: UDPClient.c UDPProxy.h
	$(CC) -o UDPClient UDPClient.c 

UDPServer: UDPServer.c UDPProxy.h
	$(CC) -o UDPServer UDPServer.c

UDPProxy: UDPClient UDPServer UDPProxy.c UDPProxy.h
	$(CC) -o UDPProxy UDPProxy.c

all: clean default


CC=gcc
CFLAGS=-g -Wall

default: UDPProxy

clean:
	rm -f *.o UDPProxy UDPClient UDPServer

UDPClient: UDPClient.c
	$(CC) -o UDPClient UDPClient.c 

UDPServer: UDPServer.c 
	$(CC) -o UDPServer UDPServer.c

UDPProxy: UDPClient UDPServer UDPProxy.c
	$(CC) -o UDPProxy UDPProxy.c

all: clean default


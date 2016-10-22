CC=gcc
CFLAGS=-g -Wall

default: UDPProxy

clean:
	rm -f *.o UDPProxy UDPClient UDPServer hashtable

UDPProxy: UDPProxy.o hashtable.o UDPSocket.o UDPServer UDPClient
	$(CC) -o UDPProxy UDPProxy.o UDPSocket.o hashtable.o 

UDPClient: UDPClient.o
	$(CC) -o UDPClient UDPSocket.o UDPClient.o

UDPServer: UDPServer.o
	$(CC) -o UDPServer UDPSocket.o UDPServer.o 

%.o: %.c
		$(CC) $(CFLAGS) -c $*.c 

all: clean default


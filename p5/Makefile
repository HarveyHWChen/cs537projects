#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o request.o cs537.o client.o
TARGET = server

CC = gcc
CFLAGS = -g -Wall

LIBS = -pthread

.SUFFIXES: .c .o 

all: server client output.cgi output10.cgi testclient

server: server.o request.o cs537.o
	$(CC) $(CFLAGS) $(LIBS) -o server server.o request.o cs537.o -lrt

client: client.o cs537.o
	$(CC) $(CFLAGS) -o client client.o cs537.o

testclient: testclient.o cs537.o
	$(CC) $(CFLAGS) -o testclient testclient.o cs537.o

output.cgi: output.c
	$(CC) $(CFLAGS) -o output.cgi output.c 

output10.cgi: output10.c
	$(CC) $(CFLAGS) -o output10.cgi output10.c 

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $< 

clean:
	-rm -f $(OBJS) server client output.cgi

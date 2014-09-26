CC   = gcc
CFLAGS = -g -Wall -Woverflow

all:  mfs.o libmfs.so server.o client.o udp.o server client

libmfs.so : mfs.o
	$(CC) $(CFLAGS) -shared -o $@ $<

server.o : server.c
	$(CC) $(CFLAGS) -c $< -o $@

client.o : client.c
	$(CC) $(CFLAGS) -c $< -o $@

udp.o : udp.c
	$(CC) $(CFLAGS) -c $< -o $@

mfs.o : mfs.c
	$(CC) $(CFLAGS) -c -fpic $< -o $@ 

server: server.o udp.o
	$(CC) $(CFLAGS) -o server server.o udp.o -lmfs -L.

client: client.o udp.o
	$(CC) $(CFLAGS) -o client client.o udp.o -lmfs -L.

clean:
	rm -f *.o server client libmfs.so


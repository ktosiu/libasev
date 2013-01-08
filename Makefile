CC 	=	gcc
CFLAGS	=	-O2 -fPIE -fstack-protector --param=ssp-buffer-size=4 \
	-Wall -W -Wshadow -Wformat-security \
	-D_FORTIFY_SOURCE=2 \

LINK	=	-Wl,-s
LDFLAGS	=	-fPIE -pie -Wl,-z,relro -Wl,-z,now

OBJS	=	server.o utility.o picoev_epoll.o sysutil.o str.o

.c.o:
	$(CC) -c $*.c $(CFLAGS)

bin/asev: $(OBJS)
	mkdir -p bin
	$(CC) -o server $(OBJS) $(LINK) $(LDFLAGS)

clean:
	rm -f *.o *.swp asev

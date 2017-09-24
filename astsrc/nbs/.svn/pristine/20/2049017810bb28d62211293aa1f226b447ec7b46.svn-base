CFLAGS=-g -Wall -O2 -D_NBS_PRIVATE -Wmissing-prototypes -Werror -Wno-pointer-sign -D_REENTRANT
OBJS=nbsd.o nbsclient.o
STATIC_OBJS=nbsclient.o
DYNAMIC_OBJS=nbsclient.lo
STATIC_LIBRARY=libnbs.a
DYNAMIC_LIBRARY=libnbs.so.1.0

all: nbsd $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY) nbscat

install: all
	mkdir -p /usr/lib
	mkdir -p /usr/sbin
	mkdir -p /usr/include
	install -m 755 nbsd /usr/sbin/nbsd
	install -m 755 nbscat /usr/bin/nbscat
	install -m 755 nbscat8k /usr/bin/nbscat8k
	install -m 644 nbs.h /usr/include/nbs.h
	install -m 755 $(DYNAMIC_LIBRARY) /usr/lib
	( cd /usr/lib ; ln -sf libnbs.so.1 libnbs.so )
	install -m 755 $(STATIC_LIBRARY) /usr/lib
	/sbin/ldconfig

%.lo : %.c
	$(CC) -fPIC $(CFLAGS) -o $@ -c $<

$(STATIC_LIBRARY): $(STATIC_OBJS)
	ar rcs $(STATIC_LIBRARY) $(STATIC_OBJS)
	ranlib $(STATIC_LIBRARY)

$(DYNAMIC_LIBRARY): $(DYNAMIC_OBJS)
	$(CC) -shared -Wl,-soname,libnbs.so.1 -o $@ $(DYNAMIC_OBJS)
	/sbin/ldconfig -n .
	ln -sf libnbs.so.1 libnbs.so

clean:
	rm -f *.o *.so.* *.so *.lo
	rm -f $(STATIC_LIBRARY) $(DYNAMIC_LIBRARY)
	rm -f nbscat nbsd

nbsd: $(OBJS)
	$(CC) -o nbsd $(OBJS) -lpthread

nbscat: nbscat.o $(DYNAMIC_LIBRARY)
	$(CC) -o nbscat nbscat.o -L. -lnbs

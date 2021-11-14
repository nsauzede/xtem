TARGET=libxtem.so
TARGET+=xtem

CFLAGS:=-Wall -Werror -Wextra
CFLAGS+=-fPIC
CFLAGS+=-g
CFLAGS+=-O0

all: $(TARGET)

librspd/librspd.h:
	git submodule update --init --recursive

libxtem.o: CFLAGS+=-Ilibrspd
libxtem.o: libxtem.c librspd/librspd.h

xtem: xtem.o libxtem.a
	$(CC) -o $@ $^ -pthread

%.so: %.o
	$(CC) -shared -o $@ $^

%.a: %.o
	$(AR) cr $@ $^

clean:
	$(RM) $(TARGET) *.so *.o *.a

clobber: clean

mrproper: clobber
	$(RM) -Rf librspd

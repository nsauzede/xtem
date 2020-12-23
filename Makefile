TARGET=libxtem.so

CFLAGS:=-Wall -Werror -Wextra
CFLAGS+=-fPIC
CFLAGS+=-g
CFLAGS+=-O0

all: $(TARGET)

pcemx: pcemx.c
	$(CC) -o $@ $^ -pthread

%.so: %.o
	$(CC) -shared -o $@ $^

%.a: %.o
	$(AR) cr $@ $^

clean:
	$(RM) $(TARGET) *.so *.o *.a

clobber: clean

mrproper: clobber

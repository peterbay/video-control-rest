PROG ?= video-control-rest
ARGS ?= -p 8800 -i 0.0.0.0

CROSS_COMPILE	?= 
CC	:= $(CROSS_COMPILE)gcc

ifeq "$(MBEDTLS_DIR)" ""
else
CFLAGS += -DMG_ENABLE_MBEDTLS=1 -I$(MBEDTLS_DIR)/include -I/usr/include
CFLAGS += -L$(MBEDTLS_DIR)/lib -lmbedtls -lmbedcrypto -lmbedx509
endif

all: $(PROG)

test: $(PROG)
	./$(PROG) $(ARGS)

$(PROG): main.c
	$(CC) mongoose.c mjson.c -W -Wall -DMG_ENABLE_LOG=0 $(CFLAGS) -o $(PROG) main.c

clean:
	rm -rf $(PROG) *.o *.dSYM *.gcov *.gcno *.gcda *.obj *.exe *.ilk *.pdb

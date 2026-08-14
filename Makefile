CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -D_GNU_SOURCE -fPIC -fno-strict-aliasing
LDFLAGS := -shared -ldl -lm

SRC    := main.c chams.c bhop.c util.c menu.c
OBJ    := $(SRC:.c=.o)
TARGET := lannister.so

HEADERS := interfaces.h chams.h bhop.h util.h menu.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

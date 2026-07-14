CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -D_GNU_SOURCE -fPIC
LDFLAGS := -shared -lpthread -lX11 -lXext -lXfixes -lSDL2

SRC    := main.c mem_internal.c game.c overlay.c
OBJ    := $(SRC:.c=.o)
TARGET := lannister.so

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

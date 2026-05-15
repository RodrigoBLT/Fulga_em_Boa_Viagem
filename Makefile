CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lgdi32

SRC = src/main.c
OUT = build/fuga-em-bv.exe

all: $(OUT)

$(OUT): $(SRC)
	@if not exist build mkdir build
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

run: all
	$(OUT)

clean:
	@if exist build rmdir /s /q build

.PHONY: all run clean


CC=gcc
FLAGS=-m32
DISABLE_WARNINGS=-Wno-pointer-sign -Wno-format
FILES=civ/*.c
OUT=bin/a.out

all: test

test: build
	./bin/a.out

build:
	mkdir -p bin/
	$(CC) $(FLAGS) -Wall $(DISABLE_WARNINGS) $(FILES) -o $(OUT)

CC=gcc
FLAGS=-m32 -no-pie -g -rdynamic
DISABLE_WARNINGS=-Wno-pointer-sign -Wno-format
FILES=src/*.c tests/*.c
OUT=bin/tests

all: test

test: build
	./$(OUT)

build:
	mkdir -p bin/
	$(CC) $(FLAGS) -Isrc/ -Wall $(DISABLE_WARNINGS) $(FILES) -o $(OUT)

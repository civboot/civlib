CC=gcc
FLAGS=-m32 -g -rdynamic
DISABLE_WARNINGS=-Wno-pointer-sign -Wno-format
FILES=civ/*.c
OUT=bin/tests

all: test

test: build
	./$(OUT)

build:
	mkdir -p bin/
	$(CC) $(FLAGS) -Wall $(DISABLE_WARNINGS) $(FILES) -o $(OUT)

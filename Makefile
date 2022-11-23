
all: test

DISABLE_WARNINGS=-Wno-pointer-sign -Wno-format

test: build
	./bin/a.out

build:
	mkdir -p bin/
	gcc -m32 -Wall $(DISABLE_WARNINGS) civ/*.c -o bin/a.out

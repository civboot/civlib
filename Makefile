
all: test

test: build
	./bin/a.out

build:
	mkdir -p bin/
	gcc -m32 civ/*.c -o bin/a.out

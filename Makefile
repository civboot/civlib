CC=gcc
FLAGS=-m32 -no-pie -g -rdynamic
DISABLE_WARNINGS=-Wno-pointer-sign -Wno-format
FILES=src/*.c tests/*.c
OUT=bin/tests

.PHONY: all test clean lua

LP = "./lua/?.lua;${LUA_PATH}"


all: test

test: lua build
	./$(OUT)

lua:
	LUA_PATH=${LP} lua tests/test_civ.lua
	LUA_PATH=${LP} lua tests/test_gap.lua
	LUA_PATH=${LP} lua tests/test_sh.lua

build:
	mkdir -p bin/
	$(CC) $(FLAGS) -Isrc/ -Wall $(DISABLE_WARNINGS) $(FILES) -o $(OUT)

installlocal:
	luarocks make lua/rockspec --local

uploadrock:
	source ~/.secrets && luarocks upload lua/*.rockspec --api-key=${ROCKAPI}


CPPFLAGS=-Wall `sdl-config --cflags --libs` -O3 -ffast-math -std=c++11 -g

all: main

main: main.o Makefile
	$(CXX) -o $@ $< $(CPPFLAGS)

.PHONY: clean
clean:
	rm -f main main.o


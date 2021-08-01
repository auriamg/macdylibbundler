DESTDIR=
PREFIX=/usr/local
CXXFLAGS=-O2 -std=c++11

CPP_FILES=$(wildcard src/*.cpp)
OBJ_FILES=$(notdir $(CPP_FILES:.cpp=.o))

all: dylibbundler

dylibbundler: $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_FILES)

%.o: src/%.cpp
	$(CXX) -c $(CXXFLAGS) -I./src $< -o $@

clean:
	rm -f *.o
	rm -f ./dylibbundler

install: dylibbundler
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp ./dylibbundler $(DESTDIR)$(PREFIX)/bin/dylibbundler
	chmod 775 $(DESTDIR)$(PREFIX)/bin/dylibbundler

.PHONY: all clean install

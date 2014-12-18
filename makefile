CPP_FILES=$(wildcard src/*.cpp)
OBJ_FILES=$(notdir $(CPP_FILES:.cpp=.o))

dylibbundler: $(OBJ_FILES)
	g++ -std=c++11 -o ./dylibbundler ./Settings.o ./DylibBundler.o ./Dependency.o ./main.o ./Utils.o
%.o: src/%.cpp
	g++ -std=c++11 -c -I./src $< -o $@

clean:
	rm -f *.o
	rm -f ./dylibbundler
	
install: dylibbundler
	cp ./dylibbundler /usr/local/bin/dylibbundler
	chmod 775 /usr/local/bin/dylibbundler

CC=g++
CXXFLAGS=-g -Wall -std=c++14
SOURCES=mish.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=Mish

# Find the Readline include directory and library
READLINE_INCLUDE=$(shell find /usr/include /usr/local/include -name readline.h -exec dirname {} \\;)
READLINE_LIB=$(shell find /usr/lib /usr/local/lib -name libreadline.a -or -name libreadline.so -exec dirname {} \\;)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CXXFLAGS) -I$(READLINE_INCLUDE) -L$(READLINE_LIB) -lreadline -o $@ $(OBJECTS)

.cpp.o:
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *o $(EXECUTABLE)

CC=g++
CXXFLAGS=-g -Wall -std=c++14
SOURCES=mish.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=Mish

# Attempt to use pkg-config to find Readline
PKG_CONFIG := $(shell command -v pkg-config 2> /dev/null)
ifdef PKG_CONFIG
READLINE_CFLAGS := $(shell pkg-config --cflags readline)
READLINE_LIBS := $(shell pkg-config --libs readline)
else
# Fallback for macOS or if pkg-config is not available
READLINE_CFLAGS=$(shell find /usr/include /usr/local/include -name readline.h -exec dirname {} \\;)
READLINE_LIBS=$(shell find /usr/lib /usr/local/lib -name libreadline.a -or -name libreadline.so -exec dirname {} \\;)
endif

ifndef READLINE_CFLAGS
READLINE_CFLAGS=$(shell find /usr/include /usr/local/include -name readline.h -exec dirname {} \;)
READLINE_LIBS=$(shell find /usr/lib /usr/local/lib -name libreadline.a -or -name libreadline.so -exec dirname {} \;)
endif



all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CXXFLAGS) -I$(READLINE_INCLUDE) -L$(READLINE_LIB) -lreadline -o $@ $(OBJECTS)

.cpp.o:
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *o $(EXECUTABLE)

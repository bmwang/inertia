#
# THIS IS THE DEFAULT MAKEFILE FOR CS184
# Author: njoubert
#

#Code   -------------------------------

### YOU CAN CHANGE THESE VARIABLES AS YOU ADD CODE:
TARGET := sweep
SOURCES := $(wildcard ./src/UCB/*.cpp) $(wildcard ./src/*.cpp)

#Libraries -------------------------------

#Check for OSX. Works on 10.5 (and 10.4 but untested)
#This line still kicks out an annoying error on Solaris.
ifeq ($(shell sw_vers 2>/dev/null | grep Mac | awk '{ print $$2}'),Mac)
	#Assume Mac
	INCLUDE := -I./include/ -I/usr/X11/include \
		-I"lib/mac/SDL.framework/Headers"
	LIBRARY := -L./lib/mac/ \
    	-L"/System/Library/Frameworks/OpenGL.framework/Libraries" \
    	-lGL -lGLU -lm -lstdc++
	FRAMEWORK := -framework GLUT -framework OpenGL -framework SDL -framework Cocoa
	MACROS := -DOSX
	PLATFORM := Mac
    SDLM := "devel-lite/SDLMain.m"
    FRAMEWORK_PATH := -F"lib/mac"
else
	#Assume X11
	INCLUDE := -I./include/ -I/usr/X11R6/include -I/sw/include \
		-I/usr/sww/include -I/usr/sww/pkg/Mesa/include
	LIBRARY := -L./lib/nix -L/usr/X11R6/lib -L/sw/lib -L/usr/sww/lib \
		-L/usr/sww/bin -L/usr/sww/pkg/Mesa/lib -lglut -lGLU -lGL -lX11 -lGLEW
	FRAMEWORK := 
	MACROS := 
	PLATFORM := *Nix
    SDLM := 
    FRAMEWORK_PATH := 
endif

#Basic Stuff -----------------------------

CC := gcc
CXX := g++
CXXFLAGS := -g -Wall -O3 -fmessage-length=0 $(INCLUDE) $(MACROS)
LDFLAGS := $(SDLM) $(FRAMEWORK) $(LIBRARY)
#-----------------------------------------

%.o : %.cpp
	@echo "Compiling .cpp to .o for $(PLATFORM) Platform:  $<" 
	@$(CXX) -c -Wall $(CXXFLAGS) $< -o $@

OBJECTS = $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	@echo "Linking .o files into:  $(TARGET)"
	@$(CXX) $(LDFLAGS) $(OBJECTS) $(INCLUDE) $(LIBRARY) $(FRAMEWORK_PATH) -o $(TARGET) -lfreeimage -lGLEW
	
all: $(TARGET)

clean:
	@echo "Removing compiled files:  $<" 
	/bin/rm -f $(OBJECTS) $(TARGET)

.DEFAULT_GOAL := all

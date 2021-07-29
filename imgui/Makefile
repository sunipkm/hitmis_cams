CC=gcc
CXX=g++
RM= /bin/rm -vf
ARCH=UNDEFINED
PWD=pwd
CDR=$(shell pwd)

EDCFLAGS:=$(CFLAGS)
EDLDFLAGS:=$(LDFLAGS)
EDDEBUG:=$(DEBUG)

ifeq ($(ARCH),UNDEFINED)
	ARCH=$(shell uname -m)
endif

UNAME_S := $(shell uname -s)

EDCFLAGS+= -I include/ -I ./ -Wall -O2 -std=gnu11 -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W
CXXFLAGS:= -I include/ -I ./ -Wall -O2 -fpermissive -std=gnu++11 -I libs/gl3w -DIMGUI_IMPL_OPENGL_LOADER_GL3W $(CXXFLAGS)
LIBS = 

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lGL `pkg-config --static --libs glfw3`
	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	CXXFLAGS:= -arch $(ARCH) $(CXXFLAGS)
	LIBS += -arch $(ARCH) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

GUILIB=libimgui_glfw.a # ImGui library with glfw backend
PLOTLIB=libimplot.a # ImPlot library (backend agnostic)

all: $(GUILIB) test testgl3 # $(PLOTLIB)
	echo Platform: $(ECHO_MESSAGE)

test: $(GUILIB) # Build the OpenGL2 test program
	$(CXX) -o test.out src/main.cpp $(CXXFLAGS) $(GUILIB) \
	$(LIBS)

testgl3: $(GUILIB) # Build the OpenGL3 test program
	$(CXX) -o test_gl3.out src/main_gl3.cpp $(CXXFLAGS) $(GUILIB) \
	$(LIBS)


$(GUILIB): # Build the ImGui library
	make -f Makefile.imgui

$(PLOTLIB): # Build the ImPlot library
	make -f Makefile.implot

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<
%.o: %.c
	$(CC) $(EDCFLAGS) -o $@ -c $<
	
.PHONY: clean

clean:
	$(RM) $(GUILIB)
	$(RM) $(PLOTLIB)
	$(RM) test.out
	$(RM) test_gl3.out

spotless: clean
	make -f Makefile.imgui spotless
	make -f Makefile.implot spotless


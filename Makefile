CC = clang
CXX = clang++
CXXFLAGS = -fPIC -std=c++17
LDFLAGS = -shared -lstdc++
# This is for the qemu plugin API header
INCLUDES = -I $(shell pwd)
PLUGIN = libibresolver.so
SRC = src/plugin.cpp
ALL_OBJS = src/plugin.o src/binaryninja_backend.o src/simple_backend.o
DEMO_SRC = backend_demo.c
DEMO_BACKEND = libdemo.so

BACKEND ?= simple
DEFINES = -DBACKEND_NAME=\"$(BACKEND)\"

ifeq ($(BACKEND), binja)
ifndef BINJA_INSTALL_DIR
$(error BINJA_INSTALL_DIR is not specified)
endif
ifndef BINJA_API_DIR
$(error BINJA_API_DIR is not specified)
endif
endif

ifeq ($(BACKEND), simple)
SRC += src/simple_backend.cpp
else ifeq ($(BACKEND), binja)
SRC += src/binaryninja_backend.cpp
DEFINES += -DBINJA_PLUGIN_DIR="\"$(BINJA_INSTALL_DIR)/plugins\""
INCLUDES += -I binaryninja-api
LDFLAGS += -L $(BINJA_API_DIR) -L $(BINJA_INSTALL_DIR) \
    -lbinaryninjaapi -lbinaryninjacore \
    -Wl,-rpath=$(BINJA_INSTALL_DIR)
else
$(error Unknown disassembly backend $(BACKEND))
endif

OBJ = $(SRC:.cpp=.o)

$(PLUGIN): $(OBJ)
	@echo Building with the $(BACKEND) disassembly backend as the default
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

demo: $(DEMO_SRC)
	$(CC) $< -shared -o $(DEMO_BACKEND)

clean:
	rm -f $(PLUGIN) $(DEMO_BACKEND) $(ALL_OBJS)


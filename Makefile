CXX = clang++
CXXFLAGS = -fPIC -std=c++17
LDFLAGS = -shared -lstdc++
# This is for the qemu plugin API header
INCLUDES = -I $(shell pwd)
PLUGIN = libibresolver.so
SRC = src/plugin.cpp

BACKEND ?= simple
DEFINES =

ifeq ($(BACKEND), binja)
ifndef BINJA_INSTALL_DIR
$(error BINJA_INSTALL_DIR is not specified)
endif
endif

ifeq ($(BACKEND), simple)
SRC += src/simple_backend.cpp
else ifeq ($(BACKEND), binja)
SRC += src/binaryninja_backend.cpp
DEFINES += -DBINJA_PLUGIN_DIR="\"$(BINJA_PLUGIN_DIR)/plugins\""
INCLUDES += -I binaryninja-api
LDFLAGS += -L build/out -L $(BINJA_INSTALL_DIR) \
    -lbinaryninjaapi -lbinaryninjacore \
    -Wl,-rpath=$(BINJA_INSTALL_DIR)
else
LIB_NAME = $(subst lib,,$(notdir $(basename $(BACKEND))))
LIB_DIR = $(abspath $(dir $(BACKEND)))
LDFLAGS += -L$(LIB_DIR)  -l$(LIB_NAME) \
    -Wl,-rpath=$(LIB_DIR)
endif

OBJ = $(SRC:.cpp=.o)

$(PLUGIN): $(OBJ)
	@echo Using the $(BACKEND) disassembly backend
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

clean:
	rm -f $(PLUGIN) $(OBJ)


CXX = clang++
CXXFLAGS = -fPIC -std=c++17
LDFLAGS = -shared -lstdc++
INCLUDES = -I $(shell pwd)
PLUGIN = libibresolver.so
SRC = src/plugin.cpp src/simple_disasm.cpp

BACKEND = simple

ifeq ($(BACKEND), binja)
ifndef BINJA_INSTALL_DIR
$(error BINJA_INSTALL_DIR is not specified)
endif
endif

BACKEND_LIST = -DSIMPLE_BACKEND=0 -DBINJA_BACKEND=1
DEFINES = $(BACKEND_LIST)

ifeq ($(BACKEND), simple)
BACKEND_NAME = SIMPLE_BACKEND
else ifeq ($(BACKEND), binja)
BACKEND_NAME = BINJA_BACKEND
SRC += src/binaryninja_disasm.cpp
BINJA_PLUGIN_DIR = $(BINJA_INSTALL_DIR)/plugins
DEFINES += -DBINJA_PLUGIN_DIR="\"$(BINJA_PLUGIN_DIR)\""
INCLUDES += -I binaryninja-api
LINK_BINJA = -L build/out -L $(BINJA_INSTALL_DIR) \
    -lbinaryninjaapi -lbinaryninjacore \
    -Wl,-rpath=$(BINJA_INSTALL_DIR)

else
$(error Unknown disassembly backend $(BACKEND))
endif

DEFINES += -DBACKEND=$(BACKEND_NAME)

OBJ = $(SRC:.cpp=.o)

$(PLUGIN): $(OBJ)
	@echo Using the $(BACKEND) disassembly backend
	$(CXX) $(LDFLAGS) $(LINK_BINJA) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

clean:
	rm -f $(PLUGIN) $(PLUGIN2) $(OBJ)


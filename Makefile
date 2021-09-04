CXX = clang++
CXXFLAGS = -fPIC -std=c++17
LDFLAGS = -shared -lstdc++

# TODO: Add alternate disassembly backend
BACKEND = binja
ifneq ($(BACKEND),binja)
$(error Unknown disassembly backend $(BACKEND))
endif

ifndef BINJA_INSTALL_DIR
$(error BINJA_INSTALL_DIR is not specified)
endif

BINJA_PLUGIN_DIR = $(BINJA_INSTALL_DIR)/plugins

DEFINES = -DBINJA_PLUGIN_DIR="\"$(BINJA_PLUGIN_DIR)\""
INCLUDES = -I binaryninja-api
LINK_BINJA = -L build/out -L $(BINJA_INSTALL_DIR) \
    -lbinaryninjaapi -lbinaryninjacore \
    -Wl,-rpath=$(BINJA_INSTALL_DIR)

PLUGIN = libibresolver.so
SRC = plugin.cpp disasm.cpp
OBJ = $(SRC:.cpp=.o)

$(PLUGIN): $(OBJ)
	@echo Using $(BACKEND) as the disassembly backend
	$(CC) $(LDFLAGS) $(LINK_BINJA) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $(DEFINES) $< -o $@

clean:
	rm -f $(PLUGIN) $(PLUGIN2) $(OBJ)


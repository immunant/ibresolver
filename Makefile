CXX = clang++
CXXFLAGS = -fPIC
LDFLAGS = -shared -lstdc++
PLUGIN = libiresolver.so
SRC = plugin.cpp
OBJ = $(SRC:.cpp=.o)

$(PLUGIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(PLUGIN) $(OBJ)


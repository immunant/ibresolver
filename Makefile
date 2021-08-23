CC = clang
CFLAGS = -fPIC
LDFLAGS = -shared
PLUGIN = libiresolver.so
SRC = plugin.c
OBJ = $(SRC:.c=.o)

$(PLUGIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -f $(PLUGIN) $(OBJ)


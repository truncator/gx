CC=clang
CC_FLAGS=-std=c11                                                              \
		 -O0 -g                                                                \
		 -Iext/include                                                         \
		 -Wall -Werror -Wextra -Wpedantic -Wshadow                             \
		 -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter       \
		 -Wno-missing-field-initializers -Wno-missing-braces
LD_FLAGS=-Lext/lib/linux64
LIBS=-lc -lm -lpthread -ldl                                                    \
	 -lGL -lglfw3 -lgl3w                                                       \
	 -lX11 -lXi -lXinerama -lXcursor -lXxf86vm -lXrandr

BINARY_DIR=bin
OBJECT_DIR=build

BINARY=gx

default: $(BINARY)

SOURCES:=$(wildcard src/*.c)
HEADERS:=$(wildcard src/*.h)
OBJECTS:=$(patsubst src/%.c,$(OBJECT_DIR)/%.o,$(SOURCES))

$(OBJECT_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p $(OBJECT_DIR)
	@$(CC) $(CC_FLAGS) -o $@ -c $<

$(BINARY): $(OBJECTS)
	@mkdir -p $(BINARY_DIR)
	@$(CC) $(LD_FLAGS) -o $(BINARY_DIR)/$(BINARY) $(OBJECTS) $(LIBS)

.PHONY: clean
clean:
	@rm -rf $(BINARY_DIR)/$(BINARY) $(OBJECT_DIR)

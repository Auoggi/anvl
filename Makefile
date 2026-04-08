MAIN_FILE := anvl

SRC_DIR := src
BUILD_DIR := .build

FLAGS := -std=c23 -D_POSIX_C_SOURCE=202405L -I $(BUILD_DIR) $(shell pkg-config --cflags --libs xkbcommon wayland-client)

PROTO_OBJS := $(patsubst protocol/%.xml, $(BUILD_DIR)/%-protocol.o, $(shell fd -e xml . protocol))
PROTO_HEADERS := $(patsubst protocol/%.xml, $(BUILD_DIR)/%-client-protocol.h, $(shell fd -e xml . protocol))

.PRECIOUS: $(BUILD_DIR)/%.o $(BUILD_DIR)/%.h $(BUILD_DIR)/%.c

$(BUILD_DIR)/$(MAIN_FILE): $(SRC_DIR)/$(MAIN_FILE).c $(PROTO_OBJS) $(PROTO_HEADERS)
	$(CC) -o $@ $^ $(FLAGS)

$(BUILD_DIR)/%-protocol.o: $(BUILD_DIR)/%-protocol.c
	$(CC) -c $(FLAGS) $^ -o $@

$(BUILD_DIR)/%.h: 
	wayland-scanner client-header < $(patsubst $(BUILD_DIR)/%-client-protocol.h, protocol/%.xml, $@) > $@

$(BUILD_DIR)/%.c: 
	wayland-scanner private-code < $(patsubst $(BUILD_DIR)/%-protocol.c, protocol/%.xml, $@) > $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

.PHONY: build
build: $(BUILD_DIR) $(BUILD_DIR)/$(MAIN_FILE)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)


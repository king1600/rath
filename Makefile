CC        := g++-7
SRC_DIR   := src
BUILD_DIR := build
INCLUDES  := -I.
LDFLAGS   := 
CFLAGS    := -g -std=c++11 -Wall -O0 -c -fPIC
EXT       := cc
BINARY    := rath

SOURCES := $(shell find $(SRC_DIR) -name '*.$(EXT)' | sort -k 1nr | cut -f2-)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.$(EXT)=$(BUILD_DIR)/%.o)
DEPS    := $(OBJECTS:.o=.d)

$(BINARY) : $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.$(EXT)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) && mkdir $(BUILD_DIR)

-include $(DEPS)
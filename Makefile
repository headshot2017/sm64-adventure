default: lib

CC      := gcc
CXX     := g++
LDFLAGS := -lm -lSDL2 -lole32 -lsetupapi -lgdi32 -lopengl32 -lwinmm -limm32 -loleaut32 -lversion -shared -static -static-libgcc -static-libstdc++
CFLAGS := -g -Wall -Wno-unused-function -Wno-unknown-pragmas -Wno-unused-variable -fPIC -DSM64_LIB_EXPORT -DGBI_FLOATS -DVERSION_US -DNO_SEGMENTED_MEMORY
INCLUDES := -Isrc/sm64-adventure/programming-gcc -Isrc/decomp/include

SRC_DIRS  := src src/sm64-adventure src/decomp src/decomp/engine src/decomp/include/PR src/decomp/game src/decomp/pc src/decomp/pc/audio src/decomp/mario src/decomp/tools src/decomp/audio
BUILD_DIR := build
DIST_DIR  := dist
ALL_DIRS  := $(addprefix $(BUILD_DIR)/,$(SRC_DIRS))

LIB_FILE   := $(DIST_DIR)/sm64-adventure.dll

C_IMPORTED := src/decomp/mario/geo.inc.c src/decomp/mario/model.inc.c
H_IMPORTED := $(C_IMPORTED:.c=.h)
IMPORTED   := $(C_IMPORTED) $(H_IMPORTED)

C_FILES   := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c)) $(C_IMPORTED)
CPP_FILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
O_FILES   := $(foreach file,$(C_FILES),$(BUILD_DIR)/$(file:.c=.o)) $(foreach file,$(CPP_FILES),$(BUILD_DIR)/$(file:.cpp=.o))
DEP_FILES := $(O_FILES:.o=.d)

DUMMY != mkdir -p $(ALL_DIRS) src/decomp/mario $(DIST_DIR)


$(filter-out src/decomp/mario/geo.inc.c,$(IMPORTED)): src/decomp/mario/geo.inc.c
src/decomp/mario/geo.inc.c: ./import-mario-geo.py
	./import-mario-geo.py

$(BUILD_DIR)/%.o: %.c $(IMPORTED)
	@$(CC) $(CFLAGS) -MM -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

$(BUILD_DIR)/%.o: %.cpp $(IMPORTED)
	@$(CXX) $(CFLAGS) -MM -MP -MT $@ -MF $(BUILD_DIR)/$*.d $<
	$(CXX) -c $(CFLAGS) $(INCLUDES) -o $@ $<

$(LIB_FILE): $(O_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)


lib: $(LIB_FILE)

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)

-include $(DEP_FILES)

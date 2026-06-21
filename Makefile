CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
LDFLAGS = -lncursesw
SRC = src/main.cpp src/game.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = super_mario

# MSYS2 puts ncurses headers in a subdirectory — use pkg-config if available
ifneq ($(shell which pkg-config 2>/dev/null),)
  CXXFLAGS += $(shell pkg-config --cflags ncursesw)
  LDFLAGS = $(shell pkg-config --libs ncursesw)
endif

all: $(TARGET)

$(TARGET): $(SRC) src/defs.h src/game.h src/level_data.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean

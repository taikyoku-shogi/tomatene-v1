.PHONY: all fast preprocess dist clean
CXX = g++
CXXFLAGS = -std=c++23 -Iinclude -Wall -Wextra -Wpedantic
SRC = tomatene.cpp
TARGET_DIR = dist
TARGET = tomatene

COMPILE = $(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET_DIR)/$(TARGET)

all: $(SRC) | dist
	$(COMPILE)

fast: $(SRC) | dist
	$(COMPILE)_fast -Ofast

preprocess: $(SRC) | dist
	$(COMPILE).i -E

dist:
	mkdir -p dist

clean:
	rm -r $(TARGET_DIR)

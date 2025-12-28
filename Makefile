.PHONY: all fast debug preprocess dist clean
CXX = g++
CXXFLAGS = -std=c++23 -Iinclude -Wall -Wextra -Wpedantic
SRC = tomatene.cpp
TARGET_DIR = dist
TARGET = tomatene

COMPILE = $(CXX) $(CXXFLAGS) $(ARGS) $(SRC) -o $(TARGET_DIR)/$(TARGET)

all: $(SRC) | dist
	$(COMPILE)

fast: $(SRC) | dist
	$(COMPILE)_fast -Ofast -march=native

debug: $(SRC) | dist
	$(COMPILE) -g -fno-omit-frame-pointer

preprocess: $(SRC) | dist
	$(COMPILE).i -E

dist:
	mkdir -p dist

clean:
	rm -r $(TARGET_DIR)

# Makefile for camera capture using OpenCV on ZCU104

# Compiler
CXX = /usr/bin/aarch64-linux-gnu-g++

# Project name
TARGET = camera

# Source files
SRCS = camera.cpp

# Compiler flags
CXXFLAGS = -O2 -std=c++11 `pkg-config --cflags opencv4`

# Linker flags
LDFLAGS = `pkg-config --libs opencv4`

# Default target
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Clean up build files
clean:
	rm -f $(TARGET)

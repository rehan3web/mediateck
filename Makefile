CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?= -lsetupapi -ladvapi32

SRC = src/main.cpp src/Logger.cpp src/SerialPort.cpp src/DeviceWatcher.cpp src/MtkHandshake.cpp
TARGET = bin/mtk_meta_tool.exe

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)

clean:
	rm -rf bin build

.PHONY: all clean

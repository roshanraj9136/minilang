CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g
SRC = src/main.cpp src/lexer.cpp src/ast.cpp src/parser.cpp src/semantic.cpp src/codegen.cpp src/opcode.cpp src/vm.cpp src/disassembler.cpp src/debugger.cpp src/transpiler.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = minilang

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) src/*.o

rebuild: clean $(TARGET)

.PHONY: clean rebuild

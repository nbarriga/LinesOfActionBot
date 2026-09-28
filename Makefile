CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra

SRCS = Board.cpp Move.cpp Types.cpp Search.cpp TimeManager.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = loabot
TEST_TARGET = test_runner

all: $(TARGET)

$(TARGET): main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_TARGET) 
	./$(TEST_TARGET)

$(TEST_TARGET): tests.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET) $(TEST_TARGET)

.PHONY: all test clean

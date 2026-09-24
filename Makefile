CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET = pseudoc

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "Testing tour.pseudo on VM..."
	@./$(TARGET) tour.pseudo > /dev/null
	@echo "Testing fizzbuzz.pseudo on VM..."
	@./$(TARGET) fizzbuzz.pseudo > /dev/null
	@echo "Testing sum.pseudo on VM..."
	@echo 10 | ./$(TARGET) sum.pseudo > /dev/null
	@echo "Testing tour.pseudo C code generation..."
	@./$(TARGET) tour.pseudo -o /tmp/tour_test.c
	@gcc /tmp/tour_test.c -o /tmp/tour_test && /tmp/tour_test > /dev/null
	@rm -f /tmp/tour_test.c /tmp/tour_test
	@echo "All tests passed successfully!"

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all test clean

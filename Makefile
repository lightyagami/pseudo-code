CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -MMD -MP
TARGET = pseudoc

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

-include $(DEPS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	@echo "=== Running Pseudoc Verification Suite ==="
	@echo "[VM] tour.pseudo"
	@./$(TARGET) tour.pseudo > /dev/null
	@echo "[VM] fizzbuzz.pseudo"
	@./$(TARGET) fizzbuzz.pseudo > /dev/null
	@echo "[VM] sum.pseudo"
	@echo 10 | ./$(TARGET) sum.pseudo > /dev/null
	@echo "[VM] test_procedures.pseudo"
	@./$(TARGET) tests/test_procedures.pseudo > /dev/null
	@echo "[VM] test_records.pseudo"
	@./$(TARGET) tests/test_records.pseudo > /dev/null
	@echo "[VM] test_case.pseudo"
	@./$(TARGET) tests/test_case.pseudo > /dev/null
	@echo "[VM] test_file_io.pseudo"
	@./$(TARGET) tests/test_file_io.pseudo > /dev/null && rm -f test_sample.txt
	@echo "[C Backend] tour.pseudo"
	@./$(TARGET) tour.pseudo -o /tmp/tour_test.c && gcc /tmp/tour_test.c -o /tmp/tour_test && /tmp/tour_test > /dev/null
	@echo "[C Backend] fizzbuzz.pseudo"
	@./$(TARGET) fizzbuzz.pseudo -o /tmp/fb_test.c && gcc /tmp/fb_test.c -o /tmp/fb_test && /tmp/fb_test > /dev/null
	@echo "[C Backend] sum.pseudo"
	@./$(TARGET) sum.pseudo -o /tmp/sum_test.c && gcc /tmp/sum_test.c -o /tmp/sum_test && echo 10 | /tmp/sum_test > /dev/null
	@echo "[C Backend] test_procedures.pseudo"
	@./$(TARGET) tests/test_procedures.pseudo -o /tmp/proc_test.c && gcc /tmp/proc_test.c -o /tmp/proc_test && /tmp/proc_test > /dev/null
	@echo "[C Backend] test_records.pseudo"
	@./$(TARGET) tests/test_records.pseudo -o /tmp/rec_test.c && gcc /tmp/rec_test.c -o /tmp/rec_test && /tmp/rec_test > /dev/null
	@echo "[C Backend] test_case.pseudo"
	@./$(TARGET) tests/test_case.pseudo -o /tmp/case_test.c && gcc /tmp/case_test.c -o /tmp/case_test && /tmp/case_test > /dev/null
	@echo "[C Backend] test_file_io.pseudo"
	@./$(TARGET) tests/test_file_io.pseudo -o /tmp/file_test.c && gcc /tmp/file_test.c -o /tmp/file_test && /tmp/file_test > /dev/null && rm -f test_sample.txt
	@rm -f /tmp/tour_test* /tmp/fb_test* /tmp/sum_test* /tmp/proc_test* /tmp/rec_test* /tmp/case_test* /tmp/file_test*
	@echo "=== All VM & C Backend Tests Passed Successfully! ==="

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) sum.c sum test_sample.txt

.PHONY: all test clean

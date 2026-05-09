CC = gcc
CXX = g++

INCLUDE_DIR = include
SRC_DIR = src
TEST_DIR = tests

LIB_SRCS = $(SRC_DIR)/queue.c \
           $(SRC_DIR)/server.c \
           $(SRC_DIR)/t_pool.c 

LIB_OBJS = $(LIB_SRCS:.c=.o)

SRCS     = $(LIB_SRCS) $(SRC_DIR)/main.c
OBJS     = $(SRCS:.c=.o)

EXECUTABLE  = main.elf
TEST_BINS   = $(TEST_DIR)/queue_test.elf

CFLAGS      = -Wall -Wextra -pthread -I$(INCLUDE_DIR)
CXXFLAGS    = -Wall -Wextra -pthread -I$(INCLUDE_DIR)
LDFLAGS     = -pthread
TEST_LDFLAGS = -pthread -lgtest -lgtest_main

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(OBJS) -o $(EXECUTABLE) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do ./$$t; done

$(TEST_DIR)/queue_test.elf: $(LIB_OBJS) $(TEST_DIR)/queue_test.o
	$(CXX) $^ -o $@ $(TEST_LDFLAGS)

clean:
	rm -f $(OBJS) $(TEST_DIR)/*.o $(TEST_BINS) $(EXECUTABLE)

.PHONY: all clean test

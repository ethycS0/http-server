CXX = gcc

INCLUDE_DIR = include
SRC_DIR = src

SRCS = $(SRC_DIR)/server.c $(SRC_DIR)/main.c
OBJS = $(SRCS:.c=.o)

EXECUTABLE = main.elf

CXXFLAGS = -I$(INCLUDE_DIR)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CXX) $(OBJS) -o $(EXECUTABLE)

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(EXECUTABLE)

.PHONY: all clean

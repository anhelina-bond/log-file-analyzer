CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

TARGET = LogAnalyzer
SRCS = main.c buffer.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
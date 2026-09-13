# Build the BMS fault handler against the mock HAL and run its tests.
#   make        -> build/bms_test
#   make test   -> build + run
#   make debug  -> build + open in gdb
#   make clean

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -g -O0 -Isrc -Itest
# AddressSanitizer + UndefinedBehaviorSanitizer: catches array overruns
# (130-cell loops!) and signed-overflow bugs at runtime instead of silently.
SAN     := -fsanitize=address,undefined -fno-omit-frame-pointer

BUILD   := build
SRCS    := src/bms.c test/mock_hal.c test/test_bms.c
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
HDRS    := src/hal.h src/bms.h test/mock_hal.h
TARGET  := $(BUILD)/bms_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(SAN) -o $@ $^

$(BUILD)/%.o: %.c $(HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN) -c -o $@ $<

test: $(TARGET)
	./$(TARGET)

debug: $(TARGET)
	gdb ./$(TARGET)

clean:
	rm -rf $(BUILD)

.PHONY: all test debug clean

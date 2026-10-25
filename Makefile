ifeq ($(origin CC),default)
CC = gcc
endif
CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -O2 -D_POSIX_C_SOURCE=200809L
DEBUG_CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -O0 -DDEBUG -D_POSIX_C_SOURCE=200809L
SANITIZE_CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -fsanitize=address,undefined -D_POSIX_C_SOURCE=200809L

# Readline support (optional, default disabled for portable fallback)
HAVE_READLINE ?= 0
ifeq ($(HAVE_READLINE), 1)
    CFLAGS += -DUSE_READLINE
    LDFLAGS += -lreadline
endif

SRC_DIR = src
BUILD_DIR = build

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/shell.c \
       $(SRC_DIR)/parser.c \
       $(SRC_DIR)/executor.c \
       $(SRC_DIR)/builtins.c \
       $(SRC_DIR)/redirection.c \
       $(SRC_DIR)/history.c \
       $(SRC_DIR)/signals.c \
       $(SRC_DIR)/util.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TARGET = minishell

.PHONY: all debug sanitize clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: clean $(TARGET)

sanitize: CFLAGS = $(SANITIZE_CFLAGS)
sanitize: clean $(TARGET)

test: $(TARGET)
	@bash tests/run_tests.sh

clean:
	@rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe test_*_tmp* *.tmp

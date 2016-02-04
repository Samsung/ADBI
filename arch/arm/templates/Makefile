SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))$(SELF_DIR)
T2C := $(SELF_DIR)/t2c

SRC := $(shell find . -maxdepth 1 -name "*.S")
OBJ := $(SRC:.S=.o)

OUT := templates.c
OUT_HEAD := $(OUT:.c=.h)

CC := $(CCv7)
CFLAGS += -march=armv7-a -ggdb3 -O0

all: $(OUT) $(OUT_HEAD)

$(OBJ): %.o : %.S
	@echo "  [TMP]  $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(OUT): $(OBJ)
	@echo "  [T2C]  $@"
	@$(T2C) -o $@ $^

$(OUT_HEAD): $(OBJ)
	@echo "  [T2H]  $@"
	@$(T2C) -o $@ -H $^

clean:
	@$(RM) $(OBJ) $(OUT) $(OUT_HEAD)

.PHONY: clean



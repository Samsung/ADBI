DIR := $(dir $(lastword $(MAKEFILE_LIST)))$(SELF_DIR)
IDK := $(DIR)/../../idk
MKINJ := $(IDK)/mkinj

NAME := dummy
SYM  := $(NAME)_injectable
SRC  := $(NAME).c
INJ  := $(SRC:.c=.inj) 
INJC := $(INJ:.inj=.inj.c) 

$(INJ): $(SRC)
	@echo "  [INJ]  $@"
	$(MKINJ) --gcc $(CC) --library $(SRC) $(NAME)

clean:
	$(RM) $(INJC) $(INJ)

.SILENT:


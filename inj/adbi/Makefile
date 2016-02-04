DIR := $(dir $(lastword $(MAKEFILE_LIST)))$(SELF_DIR)
IDK := $(DIR)/../../idk
MKINJ := $(IDK)/mkinj
TOARRAY := $(IDK)/toarray

NAME := adbi
SYM  := $(NAME)_injectable
SRC  := $(NAME).c
INJ  := $(SRC:.c=.inj) 
INJC := $(INJ:.inj=.inj.c) 

$(INJC): $(INJ)
	@echo "  [2A]   $@"
	$(TOARRAY) $(INJ) $(SYM)

$(INJ): $(SRC)
	@echo "  [INJ]  $@"
	$(MKINJ) --library $(NAME) --gcc $(CC) --features nofp -o $@ $(SRC) 

clean:
	$(RM) $(INJC) $(INJ)

.SILENT:


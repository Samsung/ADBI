CC = agcc
CFLAGS =  -ggdb3

SRC = $(wildcard *.c) 
ASM = $(wildcard *.S)
OUT = $(SRC:.c=)
ASMOUT = $(ASM:.S=)

all: $(OUT) $(ASMOUT)

$(OUT): % : %.c
	echo  '  [CC]  $@'
	$(CC) $(CFLAGS) $^ -o $@

$(ASMOUT): % : %.S
	echo  '  [CC]  $@'
	$(CC) $(CFLAGS) $^ -o $@

clean:
	echo  'Cleaning up...'
	$(RM) $(OUT) $(ASMOUT)

.SILENT:

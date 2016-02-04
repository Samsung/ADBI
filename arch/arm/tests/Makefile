# Warnings
CFLAGS += -Wall -Wextra --std=gnu99

# Generate debug symbols
CFLAGS += -ggdb -O0

################################################################################

TESTCASES  := testcases
MAIN       := main.c
OUT        := adbitest
ADBISCRIPT := tests.as

PY  := $(wildcard *.py)
ASM := $(PY:.py=.S) 
OBJ := $(ASM:.S=.o) 
MAINOBJ := $(MAIN:.c=.o)

################################################################################

all: $(OUT) $(ADBISCRIPT)

clean:
	$(RM) $(OBJ) $(MAINOBJ) $(ASM) $(TESTCASES) $(OUT) $(ADBISCRIPT) $(MAIN)
	$(RM) *.pyc

push : all
	adb push $(OUT) /data

run : push
	adb shell /data/$(OUT)

################################################################################

$(OUT): $(OBJ) $(MAINOBJ)
	@echo "  [LD]  $@"
	$(CC) $(LDFLAGS) -o $@	$^

$(ADBISCRIPT): $(OUT) $(TESTCASES)
	@echo "  [PY]  $@"
	./maketest < $(TESTCASES) > $(ADBISCRIPT)
	
$(MAINOBJ) : %.o : %.c
	@echo "  [CC]  $@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@	

$(ASM) : %.S : %.py
	@echo "  [PY]  $@"
	python $< > $@

$(MAIN): $(TESTCASES)
	@echo "  [CC]  $@"
	./makemain < $(TESTCASES) > $(MAIN)

$(TESTCASES): $(OBJ)
	@echo "  [RE]  $@"
	readelf --syms $^ | grep GLOBAL | grep -o test_.* | grep -v tinsn | sort -u > $@

$(OBJ) : %.o : %.S
	@echo "  [AS]  $@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	
################################################################################

.PHONY : all clean push run
.SILENT :
	
################################################################################


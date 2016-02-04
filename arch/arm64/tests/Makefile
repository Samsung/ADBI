# Warnings
CFLAGS += -Wall -Wextra --std=gnu99

# Generate debug symbols
CFLAGS += -ggdb -O0

# Position independent
CFLAGS += -fPIE
LDFLAGS += -pie


AS := $(CC:gcc=as)

IDK 		?= ../../../idk
MKINJ 		?= $(IDK)/mkinj
ADBICACHE	?= $(IDK)/adbicache

################################################################################

TESTCASES	:= testcases
MAIN       	:= main.c
OUT        	:= adbitest
ADBI       	:= adbitest.adbi
INJ		   	:= adbitest.inj

PY  		:= $(filter-out common.py,$(wildcard *.py))
ASM 		:= $(PY:.py=.S) 
OBJ 		:= $(ASM:.S=.o) 
MAINOBJ  	:= $(MAIN:.c=.o)
OBJCACHE	:= $(OUT).ac

################################################################################

all: $(OUT) $(INJ)

clean:
	$(RM) $(OBJ) $(MAINOBJ) $(ASM) $(OBJCACHE) $(TESTCASES) $(OUT) $(ADBI) $(INJ) $(MAIN)
	$(RM) *.pyc

push : all
	adb push $(OUT) /data
	adb push $(INJ) /data

################################################################################

$(OUT): $(OBJ) $(MAINOBJ)
	@echo "  [LD]   $@"
	$(CC) $(LDFLAGS) -o $@	$^

$(OBJCACHE): $(OUT)
	@echo "  [AC]   $@"
	$(ADBICACHE) $(OUT)

$(INJ): $(OUT) $(ADBI) $(ADBICACHE)
	@echo "  [INJ]  $@"
	$(MKINJ) --gcc $(CC) --binary /data/adbitest $(ADBI)

$(MAINOBJ) : %.o : %.c
	@echo "  [CC]   $@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@	

$(ASM) $(ADBI) : mktest.intermediate
.INTERMEDIATE: mktest.inermediate

mktest.intermediate: $(PY)
	@echo "  [MKT]  adbitest.adbi"
	./mktest

$(MAIN): $(TESTCASES)
	@echo "  [MKM]  $@"
	./mkmain < $(TESTCASES) > $(MAIN)

$(TESTCASES): $(OBJ)
	@echo "  [RE]   $@"
	readelf -W --syms $^ | grep GLOBAL | grep -o test_.* | grep -v tinsn | sort -u > $@

$(OBJ) : %.o : %.S
	@echo "  [AS]   $@"
	$(AS) -c $< -o $@
	
################################################################################

.PHONY : all clean push run
.SILENT :
	
################################################################################


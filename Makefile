########################################################################################################################
#
# 	This is the main Makefile for adbiserver.  See docs/BUILDING for a building instructions.  
#
########################################################################################################################

ARCH ?= arm64

# Set compiler path
SELF_DIR        := $(PWD)

# Multiple toolchains
TOOLCHAINDIRv7	?= $(SELF_DIR)/toolchain/
PREFIXv7 		?= $(patsubst $(TOOLCHAINDIRv7)/bin/%gcc,%, $(wildcard $(TOOLCHAINDIRv7)/bin/*-gcc))
CCv7 			:= $(TOOLCHAINDIRv7)/bin/$(PREFIXv7)gcc

TOOLCHAINDIRv8	?= $(SELF_DIR)/toolchain64/
PREFIXv8 		?= $(patsubst $(TOOLCHAINDIRv8)/bin/%gcc,%, $(wildcard $(TOOLCHAINDIRv8)/bin/*-gcc))
CCv8 			:= $(TOOLCHAINDIRv8)/bin/$(PREFIXv8)gcc

ifeq ($(ARCH),arm64)
  TOOLCHAINDIR	:= $(TOOLCHAINDIRv8)
  PREFIX 		:= $(PREFIXv8)
  CC 			:= $(CCv8)
else
  TOOLCHAINDIR	:= $(TOOLCHAINDIRv7)
  PREFIX 		:= $(PREFIXv7)
  CC 			:= $(CCv7)
endif

# export CCs for sub-make  
export CC
export CCv7
export CCv8

########################################################################################################################

ifeq ($(ARCH),arm64)
  LIBS = lib/asdd/libasdd64.a lib/capstone/libcapstone.a 
  INCDIRS = lib/asdd/include lib/capstone/include
  CFLAGS += -Wno-missing-braces
  CPPFLAGS += -DEXEC_RESTOP
  # Unbelieveble things are happen on Galaxy S6 without -fomit-frame-pointer
  # or with -O3 while tracing multiple processes 
  CFLAGS += -fomit-frame-pointer
  CFLAGS += -mfix-cortex-a53-843419 -mfix-cortex-a53-835769 
  # Uncomment to use x16 register in bl instruction trampoline.
  # Use of x16 register will save one jump (return jump), it may
  # be important if relative jump is not siutable. But it increses
  # trampoline size by 12 bytes and may cause problems if x16 is in
  # use between bl innstructions (forbidden by AAPCS).  
  #CPPFLAGS += -DUSE_X16_IN_BL_TRAMPOLINE
else
  # Disable executable stack warnings (however, it seems that the linker ignores this anyway...)
  LDFLAGS += -Wl,--no-warn-execstack
  CPPFLAGS += -DEXEC_RESTOP
  
  LIBS = lib/asdd/libasdd.a
  INCDIRS = lib/asdd/include 
endif

# Include dirs
CPPFLAGS += $(foreach dir,$(INCDIRS),-I$(dir))

# Warnings
CFLAGS += -Wall -Wextra 

# Use the gnu99 C dialect
CFLAGS += --std=gnu99

CFLAGS += -fPIE
LDFLAGS += -pie

# Remove unused symbols
CFLAGS += -fdata-sections -ffunction-sections
#LDFLAGS += --gc-sections

CPPFLAGS += -iquote$(PWD)

# Generate debug symbols
#CFLAGS += -O0 -ggdb3

# Automatically generate dependency files
CPPFLAGS += -MD

# Preinclude common.h header
CPPFLAGS += -include util/common.h   

CPPFLAGS += -DBIONIC_VSNPRINTF_WORKAROUND

# Link statically with libraries. This is not necessary, but useful for easier debugging.
static: LDFLAGS += -static

# Force executable stack (adbiserver needs this for nested functions).
LDFLAGS += -Wl,-z,execstack 

########################################################################################################################

ARCHDIR := arch/$(ARCH)

#ifneq (,$(filter $(ARCH),arm arm64))
#  ARCHCDIR := arch/arm_common
#endif

INCDIRS += $(ARCHDIR)/include
SRCDIRS += 	communication 	\
			configuration	\
			injectable 	    \
			injection   	\
			process 		\
			procutil 		\
			tracepoint 		\
			util 			\
			$(ARCHDIR)		\
			.

SUBMAKE := 	./inj/adbi_mmap				\
			./inj/adbi 				 	\
			./inj/adbi_munmap 			\
			./arch/$(ARCH)/templates	\
			./adbilog

INJECTABLES  := inj/adbi/adbi.inj.c inj/adbi_mmap/adbi_mmap.inj.c inj/adbi_munmap/adbi_munmap.inj.c 

HANDLERSRC  := $(ARCHDIR)/templates/templates.c
HANDLERHEAD := $(ARCHDIR)/templates/templates.h

SUPSRC  := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
GENSRC  := $(INJECTABLES) $(HANDLERSRC)
GENHEAD := $(HANDLERHEAD)

OUT := adbiserver
all: $(OUT) 

########################################################################################################################

SRC := $(SUPSRC) $(GENSRC)
GEN := $(GENSRC) $(GENHEAD)
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

########################################################################################################################

profile: CPPFLAGS += -DADBI_PROFILING
profile: CFLAGS += -pg 
profile: PROFOBJ := profiler.o 
profile: all

########################################################################################################################

$(SUBMAKE):
	$(MAKE) -C $@

SUBMAKE_CLEAN := $(addsuffix /clean, $(SUBMAKE))

$(SUBMAKE_CLEAN):
	@echo "  Clean up $(dir $@)..."
	$(MAKE) -C $(dir $@) clean

########################################################################################################################

$(SUPSRC) : $(SUBMAKE) 

$(OUT): $(OBJ) $(PROFOBJ) $(LIBS)
	@echo "  [LD]   $@"
	$(CC) $(LDFLAGS) -o $@ $^ $(PROFOBJ)

find-unused: $(OBJ)
	./unused-syms $(OBJ)

$(OBJ): %.o : %.c
	@echo "  [CC]   $@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(HANDLERHEAD): $(HANDLERSRC)
$(SUPSRC): 		$(HANDLERHEAD)
$(HANDLERSRC) $(INJECTABLES): $(SUBMAKE)
		
clean: $(SUBMAKE_CLEAN)
	@echo "  Clean up main directory..."
	$(RM) $(OBJ) $(DEP) $(OUT) 

########################################################################################################################

push: $(OUT)
	adb push $(OUT) /data

run: push
	adb shell /data/$(OUT)

debug: push
	adb shell gdbserver tcp:7777 /data/$(OUT)

########################################################################################################################

toolchain: toolchain.7z
	7z x -bd toolchain.7z

########################################################################################################################

.PHONY : all clean $(SUBMAKE) $(SUBMAKE_CLEAN) push run debug toolchain
.SILENT : 
	
########################################################################################################################
	
-include $(DEP)

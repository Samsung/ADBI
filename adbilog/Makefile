CPPFLAGS = -DNDEBUG
CFLAGS 	 = -Wall -Wextra -O3 -fPIE
LDFLAGS  = -pie

all: adbilog

adbilog: adbilog.o

adbilog.o: adbilog.c

clean:
	$(RM) adbilog adbilog.o

push: adbilog
	adb push adbilog /data/
	@echo 'pushed to /data/'

run: push
	@echo 'launching /data/adbilog...'
	adb shell /data/adbilog

.SILENT:
.PHONY: all clean push 

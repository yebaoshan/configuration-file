#
# makefile
# ybs, 2017-08-19 15:45
#

libname = libini.so

objects = utils_ini.o

cflags = -std=gnu99 -fpic
ldflags = -shared

all: $(objects)
	gcc $(objects) -o $(libname) $(ldflags)

$(objects): utils_ini.c
	gcc -c -o $@ $(cflags) $<

.PHONY: clean
clean :
	rm $(objects)

# vim:ft=make


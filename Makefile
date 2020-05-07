VER_FILE := version
VERSION := $(shell cat ${VER_FILE})

SOURCE=./source
CC=/usr/bin/gcc
# CC="echo"
RM=/bin/rm
# RM="echo"
#
# library
#

NAME=idscron
OUT_C-CLIENT-LIB=$(SOURCE)/common-client.o
OUT_C-LIB=$(SOURCE)/common.o

CFLAGS=-Wall -g -ggdb -D_FILE_OFFSET_BITS=64 -O2 -lpthread -lz -lrt -rdynamic

all: cronids

cronids:
	mkdir bin/
	$(CC) $(CFLAGS) -DVERSION=$(VERSION) -o ./bin/$(NAME) $(SOURCE)/$(NAME).c 
clean:
	$(RM) ./bin/idscron
ver:
	../scripts_admin/idscron.pl version
	

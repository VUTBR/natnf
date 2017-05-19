PROG_NAME := natnf
SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := ${SOURCES:.c=.o}
CC := gcc
CFLAGS += -Wall -std=c99 -w -Wextra -pedantic
NOERR := 2>/dev/null

.PHONY: all clean

all: $(PROG_NAME)

$(PROG_NAME): $(OBJS) $(HEADERS)
	$(LINK.c) $(OBJS) -o $(PROG_NAME) -L/usr/lib/x86_64-linux-gnu/ -lnetfilter_conntrack -lpthread

run: $(PROG_NAME)
	sudo ./$(PROG_NAME)

clean:
	@- $(RM) $(PROG_NAME)
	@- $(RM) $(OBJS)

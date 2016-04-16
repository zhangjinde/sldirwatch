.PHONY: all clean

TARGET:=sldirwatch_test
SOURCES:=sldirwatch.c sldirwatch_test.c
OBJS:=$(patsubst %.c, %.o, $(SOURCES))
CC:=gcc
CFLAGS:=-Wall -Wextra -Winit-self -pipe -g3 -O0 -I. -ansi -pedantic

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) -MD $(CFLAGS) -o $@ -c $<

clean:
	$(RM) $(TARGET) $(OBJS) *.d

-include $(SOURCES:.c=.d)

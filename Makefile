CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -O3
LIBS = -lX11 -lGL -lGLEW -lXrandr -lm
TARGET = zoomer

SRCS = main.c config.c screenshot.c navigation.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
	mkdir -p $(DESTDIR)/usr/local/share/$(TARGET)
	install -Dm644 *.glsl $(DESTDIR)/usr/local/share/$(TARGET)/

.PHONY: all clean install

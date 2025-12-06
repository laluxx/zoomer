CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -O3
LIBS = -lX11 -lGL -lGLEW -lXrandr -lm
TARGET = zoomer
SRCS = main.c config.c screenshot.c camera.c
OBJS = $(SRCS:.c=.o)

SYSCONFDIR ?= /etc

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	mkdir -p $(DESTDIR)$(SYSCONFDIR)/$(TARGET)
	install -Dm644 vert.glsl $(DESTDIR)$(SYSCONFDIR)/$(TARGET)/vert.glsl
	install -Dm644 frag.glsl $(DESTDIR)$(SYSCONFDIR)/$(TARGET)/frag.glsl

.PHONY: all clean install install-user

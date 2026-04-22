CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Wpedantic -g
SRCS    = src/arena.c src/ast.c src/rope.c src/lexer.c src/parser.c src/sema.c src/value.c src/env.c src/eval.c src/print_ast.c src/main.c
LDFLAGS = -lm
OBJS    = $(SRCS:.c=.o)
TARGET  = stoned

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean

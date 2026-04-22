CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Wpedantic -g -MMD -MP
SRCS    = src/arena.c src/ast.c src/rope.c src/utf8.c src/lexer.c src/parser.c src/parser_expr.c src/parser_stmt.c src/sema.c src/value.c src/env.c src/eval_support.c src/eval_dispatch.c src/eval_dispatch_primitives.c src/eval_dispatch_collections.c src/eval_dispatch_objects.c src/eval.c src/print_ast.c src/main.c
LDFLAGS = -lm
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)
TARGET  = stoned

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

test: $(TARGET)
	bash tests/run.sh

.PHONY: clean test

-include $(DEPS)

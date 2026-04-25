CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Wpedantic -g -MMD -MP
SRCS    = src/arena.c src/ast.c src/rope.c src/utf8.c src/lexer.c src/parser.c src/parser_expr.c src/parser_stmt.c src/sema.c src/value.c src/env.c src/regex.c src/eval_support.c src/eval_dispatch.c src/eval_dispatch_primitives.c src/eval_dispatch_collections.c src/eval_dispatch_objects.c src/eval.c src/print_ast.c src/main.c
REGINOLD_DIR = ../reginold
CPPFLAGS = -I$(REGINOLD_DIR)
LDFLAGS = -lm $(REGINOLD_DIR)/libreginold.a
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)
TARGET  = stoned

$(TARGET): $(OBJS) $(REGINOLD_DIR)/libreginold.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(REGINOLD_DIR)/libreginold.a:
	$(MAKE) -C $(REGINOLD_DIR) libreginold.a

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

test: $(TARGET)
	bash tests/run.sh

.PHONY: clean test

-include $(DEPS)

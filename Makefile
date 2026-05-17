CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -Wpedantic -g -MMD -MP
STONED_VERSION ?= 0.1.0-stoned-dev
STONED_RUBY_VERSION ?= 4.0.0
PREFIX ?= /usr/local
DESTDIR ?=
MISE_DATA_DIR ?= $(HOME)/.local/share/mise
MISE_VERSION ?= $(STONED_VERSION)
SRCS    = src/arena.c src/ast.c src/rope.c src/utf8.c src/lexer.c src/parser.c src/parser_expr.c src/parser_stmt.c src/sema.c src/value.c src/env.c src/regex.c src/eval_support.c src/eval_dispatch.c src/eval_dispatch_primitives.c src/eval_dispatch_collections.c src/eval_dispatch_objects.c src/eval.c src/print_ast.c src/main.c
REGINOLD_DIR = ../reginold
CPPFLAGS = -I$(REGINOLD_DIR) -DSTONED_BUILD_VERSION=\"$(STONED_VERSION)\" -DSTONED_RUBY_VERSION=\"$(STONED_RUBY_VERSION)\"
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

install: $(TARGET)
	install -d "$(DESTDIR)$(PREFIX)/bin" \
	           "$(DESTDIR)$(PREFIX)/lib/ruby/$(STONED_RUBY_VERSION)" \
	           "$(DESTDIR)$(PREFIX)/lib/ruby/gems/$(STONED_RUBY_VERSION)/gems"
	install -m 755 "$(TARGET)" "$(DESTDIR)$(PREFIX)/bin/stoned"
	ln -sf stoned "$(DESTDIR)$(PREFIX)/bin/ruby"
	install -m 755 exe/gem "$(DESTDIR)$(PREFIX)/bin/gem"
	install -m 644 rbconfig.rb "$(DESTDIR)$(PREFIX)/lib/ruby/$(STONED_RUBY_VERSION)/rbconfig.rb"

mise-install: $(TARGET)
	$(MAKE) install PREFIX="$(MISE_DATA_DIR)/installs/ruby/$(MISE_VERSION)"

test: $(TARGET)
	bash tests/run.sh

test-install: $(TARGET)
	bash tests/install_layout.sh

.PHONY: clean install mise-install test test-install

-include $(DEPS)

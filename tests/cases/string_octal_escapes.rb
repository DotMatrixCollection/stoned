puts "\0".bytes.inspect        # NUL (0)
puts "\7".bytes.inspect        # BEL (7)
puts "\033".bytes.inspect      # ESC (27)
puts "\077".bytes.inspect      # '?' (63)
puts "\177".bytes.inspect      # DEL (127)
x = 31
puts "\033[#{x}m".bytes.inspect   # interpolated: ESC [ 3 1 m
puts "\033[#{x}m".length

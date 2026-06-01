require "tempfile"

t = Tempfile.new("lines")
t.puts("a")
t.puts("b")
t.flush
t.rewind
p t.readlines
t.rewind
seen = []
t.each_line { |line| seen << line.chomp }
p seen
t.unlink

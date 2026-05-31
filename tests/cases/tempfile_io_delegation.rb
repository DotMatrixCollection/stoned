require 'tempfile'

t = Tempfile.new('iodtest')
t.write("line1\nline2\nline3\n")
t.rewind

lines = []
t.each_line { |l| lines << l.chomp }
puts lines.inspect

t.rewind
puts t.readlines.map(&:chomp).inspect

t.rewind
puts t.readline.chomp
puts t.readline.chomp

t.rewind
puts t.eof?    # false
t.read
puts t.eof?    # true

t.close
t.unlink

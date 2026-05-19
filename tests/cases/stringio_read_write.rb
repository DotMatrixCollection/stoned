require 'stringio'

# Read mode: gets/readline/eof?/pos
sio = StringIO.new("hello world\nline two\nthird\n")
p sio.gets          # "hello world\n"
p sio.gets          # "line two\n"
p sio.pos           # 21
p sio.eof?          # false
p sio.gets          # "third\n"
p sio.eof?          # true
p sio.gets          # nil

# seek / rewind
sio.rewind
p sio.pos           # 0
p sio.read(5)       # "hello"

# Write mode: pos tracking
sio2 = StringIO.new
sio2.puts "one"
sio2.puts "two"
p sio2.string       # "one\ntwo\n"

# readlines
sio3 = StringIO.new("a\nb\nc\n")
p sio3.readlines    # ["a\n","b\n","c\n"]

# each_line
sio4 = StringIO.new("x\ny\nz\n")
lines = []
sio4.each_line { |l| lines << l.chomp }
p lines              # ["x","y","z"]

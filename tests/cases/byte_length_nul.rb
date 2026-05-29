# Source-level NUL bytes via \x00 escape
s = "\x00"
puts s.bytesize
puts s.bytes.inspect
puts s.empty?
puts s.b.encoding.name

# Multi-byte with embedded NUL
t = "ab\x00cd"
puts t.bytesize
puts t.bytes.inspect
puts t.length
puts t.empty?
puts t.ascii_only?
puts t.byteslice(2).bytes.inspect
puts t.byteslice(2, 1).bytes.inspect

# Concatenation preserves NUL bytes
u = "a\x00b" + "c\x00d"
puts u.bytesize
puts u.bytes.inspect

# << appending across NUL boundary
v = "x\x00"
v << "\x00y"
puts v.bytesize
puts v.bytes.inspect

# Binary file with embedded NUL
path = "tests/fixtures/binary_nul.bin"
bin = File.open(path, "rb") { |f| f.read }
puts bin.bytesize
puts bin.bytes.inspect
puts bin.encoding.name
puts bin.byteslice(1).bytes.inspect
puts bin.byteslice(0, 2).bytes.inspect

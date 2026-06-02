# String#b returns ASCII-8BIT tagged copy
s = "hello"
p s.encoding
b = s.b
p b.encoding
p b.bytesize
p b == s

# UTF-8 string with multi-byte chars
t = "caf\xc3\xa9"
p t.encoding
p t.length
p t.bytesize

bt = t.b
p bt.encoding
p bt.bytesize
p bt.length

# force_encoding changes the tag without re-encoding
f = "hello".force_encoding("ASCII-8BIT")
p f.encoding
p f.bytesize
p f.length

# b on already-binary string stays binary
bb = b.b
p bb.encoding

# String#encoding on literal
p "test".encoding
p :sym.to_s.encoding

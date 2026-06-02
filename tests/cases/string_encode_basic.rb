# String#encode for encoding conversion
s = "hello"
p s.encoding

# encode to UTF-8 (no-op if already UTF-8)
u = s.encode("UTF-8")
p u.encoding
p u == s

# ASCII-8BIT encoding
b = s.encode("ASCII-8BIT")
p b.encoding
p b.bytesize

# Encoding.find
p Encoding.find("UTF-8")
p Encoding.find("ASCII-8BIT")
p Encoding.find("US-ASCII")

# Encoding constants
p Encoding::UTF_8
p Encoding::ASCII_8BIT
p Encoding::US_ASCII

# String with explicit encoding source
raw = "\xc3\xa9".force_encoding("UTF-8")
p raw.encoding
p raw.length
p raw.bytesize

# encode from UTF-8 to ASCII with replacement
plain = "café"
p plain.length
p plain.bytesize
ascii = plain.encode("US-ASCII", invalid: :replace, undef: :replace, replace: "?")
p ascii
p ascii.encoding

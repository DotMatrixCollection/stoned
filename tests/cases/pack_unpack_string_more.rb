bytes = [65, 66, 67].pack("C*")

p bytes.bytes
p bytes.bytesize
p bytes.unpack("C*")
p [0x1234, 0x5678].pack("n*").unpack("n*")

s = "hello"
p s.encoding
p s.valid_encoding?
p s.ascii_only?

s2 = "こんにちは"
p s2.encoding
p s2.ascii_only?
p s2.length
p s2.bytesize

s3 = "abc".b
p s3.encoding
p s3.ascii_only?

s = "abc"
p s.encoding.name
p s.force_encoding("ASCII-8BIT").encoding.name
p s.valid_encoding?

s = "a\0bc".b
p s.byteslice(1..2)
p s.byteslice(-2, 2)
p s.byteslice(99)
p s.byteslice(2, -1)
p s.byteslice(4, 1)

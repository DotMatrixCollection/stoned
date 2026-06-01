s = "a\0c".b
p s.getbyte(0)
p s.getbyte(1)
p s.getbyte(-1)
p s.getbyte(99)

t = "abc"
p t.setbyte(1, 90)
p t

u = "a\0c".b
u.setbyte(1, 90)
p u

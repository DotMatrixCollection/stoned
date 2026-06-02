s = "hello world hello"
p s.index("hello")
p s.index("hello", 1)
p s.index("hello", 6)
p s.rindex("hello")
p s.rindex("hello", 10)
p s.index(/\w+/)
p s.index(/\w+/, 6)
p s.index("xyz")

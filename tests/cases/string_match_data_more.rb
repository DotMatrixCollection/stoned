m = "hello world".match(/(\w+) (\w+)/)
p m[0]
p m[1]
p m[2]
p m.pre_match
p m.post_match

m2 = "hello".match(/\d+/)
p m2
p m2.nil?

require "ostruct"

o = OpenStruct.new(a: 1)
o.b = 2
p o.to_h
p o.a

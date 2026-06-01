require "ostruct"

o = OpenStruct.new
o.name = "Ada"
o.city = "London"
p o[:name]
p o.to_h

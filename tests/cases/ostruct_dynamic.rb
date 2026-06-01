require "ostruct"

o = OpenStruct.new({name: "Ada"})
puts o.name
o.age = 37
p o[:age]
p o.respond_to?(:age)
p o.to_h
puts o.inspect

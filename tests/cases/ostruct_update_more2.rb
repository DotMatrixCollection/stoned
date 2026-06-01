require "ostruct"

o = OpenStruct.new
o.name = "stoned"
p o.name
p o.respond_to?(:name)

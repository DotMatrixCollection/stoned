module Outer
  Answer = 1
end

puts Module.constants.include?(:Object)
puts Outer.constants.include?(:Answer)

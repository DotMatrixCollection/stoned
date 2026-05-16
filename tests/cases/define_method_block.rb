module Toolbox
  class << self
    define_method(:twice) do |x|
      x * 2
    end unless method_defined?(:twice)
    name = :triple
    define_method name do |x|
      x * 3
    end unless method_defined?(name)
    puts method_defined?(:twice)
    puts method_defined?(:triple)
  end
end

class Box
  define_method(:value) do
    9
  end
end

puts Toolbox.twice(6)
puts Toolbox.triple(5)
puts Box.new.value
puts Box.method_defined?(:value)

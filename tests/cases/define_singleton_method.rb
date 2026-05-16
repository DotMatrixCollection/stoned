obj = Object.new
obj.define_singleton_method(:greet) { "hello" }
puts obj.greet
obj.define_singleton_method(:double) { |x| x * 2 }
puts obj.double(21)
puts obj.singleton_methods.sort.inspect
other = Object.new
puts other.respond_to?(:greet)

require "singleton"

class Once
  include Singleton
end

a = Once.instance
b = Once.instance

puts a.class == Once
puts a.equal?(b)

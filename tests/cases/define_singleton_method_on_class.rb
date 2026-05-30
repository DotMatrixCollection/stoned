# define_singleton_method works when receiver is a class
class Greeter
end

Greeter.define_singleton_method(:hello) { |name| "Hello, #{name}!" }
puts Greeter.hello("world")

# define_singleton_method inside a class body (self is the class)
class Counter
  @count = 0
  define_singleton_method(:increment) { @count += 1 }
  define_singleton_method(:value) { @count }
end

Counter.increment
Counter.increment
puts Counter.value

# singleton_class.define_method routes to class methods
class Config
end
Config.singleton_class.define_method(:timeout) { 30 }
puts Config.timeout

# super in initialize to built-in ancestor is a no-op, not an error
class MyObj
  def initialize(x)
    super rescue nil
    @x = x
  end
  def x; @x; end
end
puts MyObj.new(99).x

# Module#prepend inserts module before the class in method lookup
module Logging
  def greet(name)
    puts "calling greet"
    result = super
    puts "done"
    result
  end
end

class Greeter
  prepend Logging
  def greet(name) = "Hello, #{name}!"
end

g = Greeter.new
p g.greet("World")

# ancestors shows prepended module before class
p Greeter.ancestors.first(3)

# prepend affects instance method lookup
module Upcaser
  def shout = super.upcase
end

class Talker
  prepend Upcaser
  def shout = "hello"
end

p Talker.new.shout

# Multiple prepends stack in order
module A
  def tag = "A(#{super})"
end
module B
  def tag = "B(#{super})"
end
class Base
  def tag = "base"
end
class Child < Base
  prepend A
  prepend B
end
p Child.new.tag
p Child.ancestors.first(4)

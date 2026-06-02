# module_function creates both module-level and private instance methods
module MathUtils
  module_function

  def square(x) = x * x
  def cube(x)   = x * x * x
end

# Callable directly on module
p MathUtils.square(4)
p MathUtils.cube(3)

# Also available as private instance methods when included
class Calculator
  include MathUtils

  def compute(n)
    square(n) + cube(n)
  end
end

p Calculator.new.compute(2)

# module_function for specific methods
module Formatter
  def titleize(s)
    s.split.map(&:capitalize).join(" ")
  end
  module_function :titleize

  def shout(s)
    s.upcase + "!"
  end
end

p Formatter.titleize("hello world")

# Instance method version is private
begin
  Formatter.new.titleize("test")
rescue NoMethodError => e
  p e.message.include?("private")
end

# self.method form vs module_function
module Standalone
  def self.greet(name) = "Hi, #{name}"
end
p Standalone.greet("Alice")

# module_function methods are not affected by include visibility override
module Ops
  module_function
  def double(x) = x * 2
end
p Ops.double(7)

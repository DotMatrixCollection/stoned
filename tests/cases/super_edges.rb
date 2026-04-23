# super edge cases

# super through multiple module layers
module A
  def greet; "A"; end
end

module B
  def greet; "B+#{super}"; end
end

module C
  def greet; "C+#{super}"; end
end

class Root
  def greet; "Root"; end
end

class Top < Root
  include A
  include B
  include C
  def greet; "Top+#{super}"; end
end

puts Top.new.greet  # Top+C+B+A  (A has no super call so Root never reached)

# super in initialize
class Animal
  attr_reader :name
  def initialize(name)
    @name = name
  end
end

class Dog < Animal
  attr_reader :breed
  def initialize(name, breed)
    super(name)
    @breed = breed
  end
end

d = Dog.new("Rex", "Lab")
puts d.name   # Rex
puts d.breed  # Lab

# super with zero-arg method
class Counter
  def count; 0; end
end

class DoubleCounter < Counter
  def count; super + super; end
end

puts DoubleCounter.new.count  # 0

# super in a module method calling up to class
module Stamper
  def stamp; "#{super}-stamped"; end
end

class Doc
  include Stamper
  def stamp; "doc"; end
end

class Invoice < Doc
  def stamp; super; end
end

puts Invoice.new.stamp  # doc  (super finds Doc#stamp; Doc doesn't call super so Stamper never reached)

# super raises NoMethodError when no parent implementation
class Orphan
  def speak
    begin
      super
    rescue NoMethodError => e
      "no super: #{e.class}"
    end
  end
end

puts Orphan.new.speak  # no super: NoMethodError

# protected method accessible via super
class Shape
  protected
  def area_str; "Shape"; end
end

class Circle < Shape
  protected
  def area_str; "Circle>#{super}"; end
  public
  def describe; area_str; end
end

puts Circle.new.describe  # Circle>Shape

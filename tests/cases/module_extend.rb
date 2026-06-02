# extend mixes module methods into a single object
module Greetable
  def greet = "Hello, I am #{name}"
  def farewell = "Goodbye from #{name}"
end

class Person
  attr_reader :name
  def initialize(n) = @name = n
end

alice = Person.new("Alice")
alice.extend(Greetable)
p alice.greet
p alice.farewell

bob = Person.new("Bob")
p bob.respond_to?(:greet)

# extend on plain object
obj = Object.new
module Stamped
  def stamp = "stamped!"
end
obj.extend(Stamped)
p obj.stamp
p obj.is_a?(Stamped)

# extend at class level (adds class methods)
module ClassMethods
  def species = "Homo sapiens"
end
class Human
  extend ClassMethods
end
p Human.species
p Human.respond_to?(:species)
p Human.new.respond_to?(:species)

# self.extended hook
module Trackable
  def self.extended(base)
    puts "extended into #{base.class}"
  end
end
str = "hello"
str.extend(Trackable)

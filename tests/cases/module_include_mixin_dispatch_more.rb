module Greetable
  def greet = "Hello, I am #{name}"
end
class Person
  include Greetable
  attr_reader :name
  def initialize(n) = (@name = n)
end
p Person.new("Alice").greet
p Person.include?(Greetable)
p Person.ancestors.include?(Greetable)
p Person.new("Bob").is_a?(Greetable)

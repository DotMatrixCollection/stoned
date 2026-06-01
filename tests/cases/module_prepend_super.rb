module Logging
  def greet
    "LOG: " + super
  end
end

module Shouting
  def greet
    super.upcase
  end
end

class Person
  def greet
    "hello"
  end
end

class LoudPerson < Person
  prepend Shouting
  include Logging
end

p LoudPerson.new.greet
p LoudPerson.ancestors.first(5).map(&:to_s)

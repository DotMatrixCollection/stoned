module Wrapper
  def greet
    super + "!"
  end
end

class Base
  def greet
    "hi"
  end
end

class Child < Base
  include Wrapper
end

puts Child.new.greet

module ClassMethods
  def answer
    42
  end
end

class Toolbox
  extend ClassMethods
end

puts Toolbox.answer

module Extra
  def ping
    "pong"
  end
end

thing = Object.new
thing.extend Extra
puts thing.ping

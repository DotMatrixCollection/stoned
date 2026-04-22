module Decorator
  def greet
    super + "!"
  end
end

class Greeter
  def greet
    "hi"
  end

  prepend Decorator
end

puts Greeter.new.greet

module A
  def marker
    "a"
  end
end

module B
  def marker
    "b"
  end
end

class Picked
  prepend A
  prepend B

  def marker
    "class"
  end
end

puts Picked.new.marker

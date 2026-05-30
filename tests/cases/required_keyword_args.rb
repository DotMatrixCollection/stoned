# Required keyword args must not be counted against positional arity
def foo(name, bar:)
  "#{name} #{bar}"
end

puts foo("hello", bar: 99)

# From within method body
class C
  def greet(name, salutation:)
    "#{salutation}, #{name}"
  end

  def run
    greet("Alice", salutation: "Hello")
  end
end

puts C.new.run

# Via include
module M
  def tagged(value, tag:)
    "[#{tag}] #{value}"
  end
end

class D
  include M

  def run
    tagged("item", tag: "info")
  end
end

puts D.new.run

# Missing required kwarg raises
begin
  foo("hello")
rescue ArgumentError => e
  puts e.message
end

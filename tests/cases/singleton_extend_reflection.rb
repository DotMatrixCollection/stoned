module Greetable
  def greet; "hello"; end
end

module Farewell
  def bye; "bye"; end
end

class Foo
end

obj = Foo.new
obj.extend(Greetable)

puts obj.respond_to?(:greet)
puts obj.singleton_class.include?(Greetable)
puts obj.singleton_class.ancestors.include?(Greetable)

obj2 = Foo.new
obj2.extend(Greetable)
obj2.extend(Farewell)
puts obj2.singleton_class.include?(Greetable)
puts obj2.singleton_class.include?(Farewell)
puts obj2.singleton_class.ancestors.include?(Greetable)
puts obj2.singleton_class.ancestors.include?(Farewell)

# Unextended objects have the regular class as singleton class ancestor
obj3 = Foo.new
puts obj3.singleton_class.ancestors.include?(Foo)

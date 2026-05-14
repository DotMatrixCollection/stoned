p 42.is_a?(Comparable)
p "hi".is_a?(Comparable)
p 3.14.is_a?(Comparable)
p [1, 2].is_a?(Enumerable)
p({a: 1}.is_a?(Enumerable))
p (1..5).is_a?(Enumerable)

module Greetable
  def greet; "hi"; end
end

class Foo
  include Greetable
end

p Foo.new.is_a?(Greetable)
p Foo.include?(Greetable)
p Foo.new.is_a?(Comparable)

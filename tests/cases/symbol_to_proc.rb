puts [1, 2, 3].map(&:to_s).inspect
puts ["hello", "world"].map(&:upcase).inspect
puts [1, nil, false, 2, nil, 3].select(&:itself).inspect
puts [1, 2, 3, 4].inject(&:+)
puts [1, 2, 3, 4].inject(&:*)
puts [:a, :b, :c].map(&:to_s).inspect

f = :upcase.to_proc
puts f.lambda?
puts f.arity
puts f.call("hello")
puts f.call("world")

double = proc { |x| x * 2 }
puts [1, 2, 3].map(&double).inspect

def greet
  yield "World" if block_given?
end
greet { |name| puts "Hello #{name}" }
greet(&nil)
puts "done"

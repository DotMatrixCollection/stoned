# respond_to? as bare call — top-level and inside methods
puts respond_to?(:puts)    # true
puts respond_to?(:p)       # true
puts respond_to?(:nope)    # false

def greet(name); "Hi #{name}"; end
puts respond_to?(:greet)   # true

# Inside a method body
def check_responds
  puts respond_to?(:puts)    # true
  puts respond_to?(:greet)   # true
  puts respond_to?(:nope)    # false
end
check_responds

# On an object
class Foo
  def bar; end
  private
  def secret; end
end
f = Foo.new
puts f.respond_to?(:bar)     # true
puts f.respond_to?(:secret)  # false
puts f.respond_to?(:secret, true)  # true (include private)
puts f.respond_to?(:missing) # false

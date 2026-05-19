# method() for top-level user-defined methods
def greet(name)
  "Hello, #{name}"
end

def add(a, b)
  a + b
end

m = method(:greet)
puts m.class          # Method
puts m.call("World")  # Hello, World
puts m.("Ruby")       # Hello, Ruby
puts m.arity          # 1

m2 = method(:add)
puts m2.call(3, 4)    # 7
puts m2.arity         # 2

# method() for kernel functions
m3 = method(:puts)
puts m3.class         # Method

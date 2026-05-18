$stdout.sync = true

# Object#equal? checks pointer identity, not value equality

a = "hello"
b = "hello"
puts a == b         # true  (value equal)
puts a.equal?(b)    # false (different objects)

# Same object is equal? to itself
puts a.equal?(a)    # true

# Symbols and integers are value types — equal? matches ==
puts :sym.equal?(:sym)   # true
puts 42.equal?(42)        # true
puts 42.equal?(43)        # false

# Arrays: same array reference
arr1 = [1, 2, 3]
arr2 = [1, 2, 3]
arr3 = arr1
puts arr1.equal?(arr2)    # false
puts arr1.equal?(arr3)    # true

# Hash: same reference
h1 = {a: 1}
h2 = {a: 1}
h3 = h1
puts h1.equal?(h2)    # false
puts h1.equal?(h3)    # true

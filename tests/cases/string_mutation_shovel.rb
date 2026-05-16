code = +"hello"
code << " world"
puts code
a = "foo"
b = a
a << "bar"
puts a
puts b  # still "foo" in stoned (value semantics)

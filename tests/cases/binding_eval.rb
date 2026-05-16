x = 10
b = binding

puts Binding === b
puts b.local_variable_get(:x)
b.local_variable_set(:x, 25)
puts x

c = b.dup
c.local_variable_set(:y, 7)
puts eval("x + y", c)

file, line = b.source_location
puts File.basename(file)
puts line > 0
puts eval("x", b, "fake_eval.rb", 30)

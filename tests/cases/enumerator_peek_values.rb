e = [1, 2].each
puts e.peek
puts e.peek_values.inspect
puts e.next

puts e.method(:peek_values).arity
puts e.method(:with_index).arity

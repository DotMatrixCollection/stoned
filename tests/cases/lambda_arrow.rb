inc = -> (x) { x + 1 }
puts inc.call(4)
puts inc.lambda?
puts inc.arity

def arrow_return
  fn = -> (x) { return x + 2 }
  value = fn.call(5)
  puts value
  9
end

puts arrow_return

begin
  inc.call
rescue ArgumentError => e
  puts e.class
end

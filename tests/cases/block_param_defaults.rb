def via_yield
  yield
end

[1].each do |x = 5|
  puts x
end

via_yield do |x = 7|
  puts x
end

p1 = proc { |x = 9| x }
puts p1.call
puts p1.call(4)
puts p1.arity

l1 = lambda { |x = 11| x }
puts l1.call
puts l1.call(6)
puts l1.arity

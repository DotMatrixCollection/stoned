e = 3.times
puts e.class
puts e.to_a.inspect
puts e.next
puts e.next

e2 = 1.upto(4)
puts e2.class
puts e2.to_a.inspect

e3 = 5.downto(3)
puts e3.class
puts e3.to_a.inspect

# still works with block
result = []
3.times { |i| result << i }
puts result.inspect

result2 = []
2.upto(5) { |i| result2 << i }
puts result2.inspect

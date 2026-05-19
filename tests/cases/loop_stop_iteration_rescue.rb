e = [1, 2, 3].each
results = []
loop do
  results << e.next
end
puts results.inspect

e2 = [10].each
e2.next
begin
  e2.next
rescue StopIteration => ex
  puts ex.message
end

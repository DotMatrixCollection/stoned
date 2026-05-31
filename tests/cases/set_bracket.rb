require 'set'
s = Set[1, 2, 3, 2, 1]
puts s.to_a.sort.inspect
puts s.size

# operations work on Set[] too
t = Set[3, 4, 5]
puts (s | t).to_a.sort.inspect
puts (s & t).to_a.sort.inspect
puts (s - t).to_a.sort.inspect

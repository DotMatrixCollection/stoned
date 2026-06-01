3.times { |i| print "#{i} " }
puts

result = []
1.upto(5) { |i| result << i }
p result

result2 = []
5.downto(1) { |i| result2 << i }
p result2

result3 = 1.upto(5).to_a
p result3

result4 = 10.downto(6).to_a
p result4

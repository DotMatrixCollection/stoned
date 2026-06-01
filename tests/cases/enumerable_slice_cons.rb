p [1,2,3,4,5,6,7].each_slice(3).to_a
p [1,2,3,4,5].each_cons(3).to_a

result = []
(1..10).each_slice(4) { |s| result << s.sum }
p result

result2 = []
[1,2,3,4,5].each_cons(2) { |a,b| result2 << (a + b) }
p result2

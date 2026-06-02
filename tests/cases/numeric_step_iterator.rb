# Numeric#step iterates from receiver to limit by step
result = []
1.step(10, 2) { |n| result << n }
p result

result2 = []
0.step(1.0, 0.25) { |n| result2 << n.round(2) }
p result2

# step with keyword args
result3 = []
1.step(by: 3, to: 15) { |n| result3 << n }
p result3

# step downward (negative step)
result4 = []
10.step(1, -3) { |n| result4 << n }
p result4

# step returns enumerator when no block given
enum = 1.step(5, 1)
p enum.class
p enum.to_a

# Integer#upto and #downto (related)
ups = []
1.upto(5) { |n| ups << n }
p ups

downs = []
5.downto(1) { |n| downs << n }
p downs

# times returns enumerator
p 3.times.to_a

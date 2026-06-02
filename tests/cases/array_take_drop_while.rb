a = [1, 2, 3, 4, 5, 1, 2]

p a.take_while { |n| n < 4 }
p a.drop_while { |n| n < 4 }

# take_while stops at first false, drop_while resumes from first false
p [1, 3, 5, 2, 4].take_while(&:odd?)
p [1, 3, 5, 2, 4].drop_while(&:odd?)

# Range-based
p (1..10).take_while { |n| n < 5 }.to_a
p (1..10).drop_while { |n| n < 5 }.to_a

# Empty results
p [1, 2, 3].take_while { |n| n > 10 }
p [1, 2, 3].drop_while { |n| n < 10 }

# All pass take_while
p [1, 2, 3].take_while { |n| n < 10 }

# Blockless returns Enumerator
p a.take_while.class
p a.drop_while.class

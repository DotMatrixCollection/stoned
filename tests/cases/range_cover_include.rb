# Range#cover? vs Range#include? / #member?
r = (1..10)
p r.cover?(5)
p r.cover?(10)
p r.cover?(11)
p r.cover?(0)
p r.include?(5)

# cover? works on non-discrete ranges (faster, no iteration)
fr = (1.0..2.0)
p fr.cover?(1.5)
p fr.cover?(2.0)
p fr.cover?(2.1)

# include? on float range iterates — cover? does not
p (1.0..2.0).cover?(1.999)

# Exclusive range
excl = (1...10)
p excl.cover?(9)
p excl.cover?(10)
p excl.include?(10)

# String ranges
p ("a".."z").cover?("m")
p ("a".."z").cover?("A")
p ("a"..."z").include?("z")
p ("a"..."z").include?("y")

# Range cover? with another range (Ruby 2.6+)
big = (1..100)
p big.cover?(20..50)
p big.cover?(50..101)

# each on range
sum = 0
(1..5).each { |n| sum += n }
p sum

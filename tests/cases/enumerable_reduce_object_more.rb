p [1, 2, 3].reduce(:+)
p [1, 2, 3].inject(10) { |sum, x| sum + x }
p [1, 2, 3].each_with_object([]) { |x, a| a << x * 2 }

result = []
(1..6).each_cons(3) { |c| result << c }
p result
p (1..5).each_cons(2).map { |a, b| a + b }
p (1..4).each_cons(2).to_a

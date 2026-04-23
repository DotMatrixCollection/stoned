p2 = proc { |a, b| p [a, b] }
p3 = proc { |a, b, c| p [a, b, c] }
pd = proc { |(a, b), c| p [a, b, c] }
l2 = ->(a, b) { p [a, b] }

puts "--direct"
print "proc_two:"; p2.call([1, 2])
print "proc_three:"; p3.call([1, 2, 3])
print "proc_mixed:"; pd.call([1, 2])
print "lambda_two:"; begin; l2.call([1, 2]); rescue => e; puts e.class; puts e.message; end

puts "--iter"
print "block:"; [[1, 2]].each { |a, b| p [a, b] }
print "proc_pass:"; [[1, 2]].each(&p2)

# Integer: identity / predicates
puts 5.to_i
puts 5.to_f
puts 5.to_s
puts 5.to_s(2)
puts 255.to_s(16)
puts 5.integer?
puts 5.positive?
n = -3
puts n.negative?
puts 0.zero?
puts 0.nonzero?.inspect
puts 7.nonzero?
puts 5.even?
puts 4.even?
puts 5.odd?

# Integer: arithmetic helpers
puts 5.succ
puts 5.pred
puts 5.abs
neg = -5
puts neg.abs
puts 5.abs2
puts 5.ceil
puts 5.floor
puts 5.round
puts 5.truncate

# Integer: gcd / lcm / pow / divmod / digits / chr
puts 12.gcd(8)
puts 12.lcm(8)
puts 0.gcd(5)
puts 2.pow(10)
puts 2.pow(10, 1000)
puts 10.divmod(3).inspect
neg10 = -10
puts neg10.divmod(3).inspect
puts 123.digits.inspect
puts 255.digits(16).inspect
puts 65.chr

# Integer: step / between? / clamp
2.step(6, 2) { |i| print "#{i} " }
puts
puts 3.between?(1, 5)
puts 6.between?(1, 5)
puts 3.clamp(1, 5)
puts 0.clamp(1, 5)
puts 9.clamp(1, 5)

# Float: identity / predicates
puts 3.14.to_f
puts 3.14.to_i
puts 3.14.to_s
puts 3.14.integer?
puts 3.14.positive?
fn = -1.5
puts fn.negative?
puts 0.0.zero?
puts 0.0.nonzero?.inspect
puts 1.5.nonzero?

# Float: nan? / finite? / infinite?
nan = 0.0 / 0.0
inf = 1.0 / 0.0
ninf = -1.0 / 0.0
puts nan.nan?
puts inf.infinite?
puts ninf.infinite?
puts 3.14.infinite?.inspect
puts 3.14.finite?
puts inf.finite?

# Float: ceil / floor / round / truncate with ndigits
puts 3.14159.ceil
puts 3.14159.floor
puts 3.14159.round
puts 3.75.round
puts 3.14159.ceil(2)
puts 3.14159.floor(2)
puts 3.14159.round(2)
puts 3.14159.truncate(2)
ftrunc = -3.7
puts ftrunc.truncate

# Float: abs / abs2 / divmod / between? / clamp / step
fabs = -1.5
puts fabs.abs
puts 2.5.abs2
puts 7.5.divmod(2.5).inspect
puts 3.0.between?(1.0, 5.0)
puts 3.5.clamp(1.0, 3.0)
1.0.step(3.0, 1.0) { |i| print "#{i} " }
puts

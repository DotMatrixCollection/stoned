c = Complex(3, 4)
p c.abs
p c.conjugate.to_s
p (c + Complex(1, -1)).to_s
p c.polar.map { |x| x.round(5) }

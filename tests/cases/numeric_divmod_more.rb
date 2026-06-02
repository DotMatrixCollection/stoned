# divmod, modulo, remainder, and related numeric ops
p 13.divmod(4)
p (-13).divmod(4)
p 13.divmod(-4)
p (-13).divmod(-4)

# modulo (%) follows divmod sign convention
p 13 % 4
p (-13) % 4
p 13 % (-4)

# remainder truncates toward zero (C-style)
p 13.remainder(4)
p (-13).remainder(4)
p 13.remainder(-4)

# abs
p (-42).abs
p 42.abs
p (-3.14).abs

# abs2 (for complex-style)
p 3.abs2
p (-4).abs2

# Integer#pow with modulus
p 2.pow(10)
p 2.pow(10, 1000)
p 3.pow(100, 97)

# Numeric#nonzero?
p 5.nonzero?
p 0.nonzero?
p 0.0.nonzero?
p 3.14.nonzero?

# zero?
p 0.zero?
p 0.0.zero?
p 1.zero?

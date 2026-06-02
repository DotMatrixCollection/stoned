# Float::INFINITY and Float::NAN
p Float::INFINITY
p -Float::INFINITY
p Float::NAN

p Float::INFINITY.infinite?
p (-Float::INFINITY).infinite?
p 1.0.infinite?

p Float::INFINITY.finite?
p 1.0.finite?

p Float::NAN.nan?
p 0.0.nan?
p Float::INFINITY.nan?

# Arithmetic with infinity
p Float::INFINITY + 1
p Float::INFINITY * 2
p Float::INFINITY - Float::INFINITY
p 1.0 / 0.0

# Comparisons
p Float::INFINITY > 1_000_000
p -Float::INFINITY < -1_000_000
p Float::NAN == Float::NAN
p Float::NAN != Float::NAN

# Integer division by zero raises; float does not
begin
  1 / 0
rescue ZeroDivisionError => e
  p e.class
end
p 1.0 / 0.0

# Float rounding methods
p 3.7.ceil
p 3.2.ceil
p (-3.2).ceil
p (-3.7).ceil

p 3.7.floor
p 3.2.floor
p (-3.2).floor
p (-3.7).floor

p 3.7.truncate
p 3.2.truncate
p (-3.7).truncate
p (-3.2).truncate

p 3.5.round
p 3.4.round
p 3.7.round
p (-3.5).round

# With precision argument
p 3.14159.round(2)
p 3.14159.ceil(2)
p 3.14159.floor(2)
p 3.14159.truncate(2)

# Negative precision rounds to tens/hundreds
p 123.456.round(-1)
p 123.456.round(-2)

# Float#abs
p (-3.14).abs
p 3.14.abs
p 0.0.abs

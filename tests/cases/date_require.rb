require 'date'

d = Date.new(2024, 1, 15)
p d.year
p d.month
p d.day
p d.to_s
p d.strftime("%B %d, %Y")
p d.strftime("%d/%m/%Y")

d2 = d + 10
p d2.to_s

p (d2 - d)
p (Date.new(2024, 12, 31) + 1).to_s
p Date.new(2024, 1, 1).leap?
p Date.new(2023, 1, 1).leap?

p Date.parse("2024-06-20").to_s
p (Date.new(2024,1,1) < Date.new(2024,2,1))
p (Date.new(2024,2,1) == Date.new(2024,2,1))
p Date.new(2024, 1, 15).wday

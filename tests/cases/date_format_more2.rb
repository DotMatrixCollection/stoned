require "date"

d = Date.new(2024, 2, 29)
p d.leap?
p d.strftime("%Y-%m-%d")
p d.next_day(2).to_s

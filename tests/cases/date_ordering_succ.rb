require "date"

a = Date.new(2026, 6, 1)
b = Date.new(2026, 6, 2)
p a < b
p b > a
p(a <=> b)
p a.succ.to_s

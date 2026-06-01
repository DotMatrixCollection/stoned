require "date"

p (Date.new(2024, 2, 28) + 1).to_s
p (Date.new(2024, 3, 1) - 1).to_s
p (Date.new(2025, 1, 1) - Date.new(2024, 12, 31))

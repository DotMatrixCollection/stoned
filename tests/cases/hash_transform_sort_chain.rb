prices = { apple: 1.5, banana: 0.75, cherry: 3.0 }
p prices.transform_values { |v| (v * 100).to_i }
p prices.sort_by { |_, v| v }.map { |k, _| k }
p prices.transform_values { |v| v * 2 }.select { |_, v| v > 2.0 }

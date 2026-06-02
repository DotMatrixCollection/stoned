left = {a: 1, b: 2}
right = {b: 5, c: 8}

merged = left.merge(right) { |key, old_value, new_value| "#{key}:#{old_value + new_value}" }

p merged
p merged.transform_keys { |key| key.to_s.upcase }
p merged.transform_values { |value| value.to_s }
p merged.select { |key, value| key != :a && value.to_s.include?("7") }

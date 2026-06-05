h = {one: 1, two: 2}

p h.transform_keys { |k| k.to_s.upcase }.transform_values { |v| v * 10 }
p h

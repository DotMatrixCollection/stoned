h = {a: 1, b: 2, c: 3, d: 4}
p h.select { |k, v| v > 2 }
p h.reject { |k, v| v > 2 }
p h.map { |k, v| [k, v * 2] }.to_h
p h.flat_map { |k, v| [k, v] }
p h.group_by { |k, v| v.even? ? :even : :odd }

h = {a: 1, b: 2, c: 3, d: 4, e: 5}
even_odd = h.group_by { |k, v| v.even? ? :even : :odd }
p even_odd.keys.sort.map(&:to_s)
p even_odd[:even].map { |k,v| k }.sort
p even_odd[:odd].map { |k,v| k }.sort

p h.count
p h.count { |k, v| v > 2 }
p h.any? { |k, v| v > 4 }
p h.all? { |k, v| v > 0 }
p h.none? { |k, v| v > 5 }

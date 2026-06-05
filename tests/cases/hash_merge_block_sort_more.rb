h = { a: 1, b: 2 }
p h.merge({ b: 5, c: 7 }) { |key, old, new| old + new }
p h.sort_by { |pair| pair[1] }.map { |pair| pair[0] }

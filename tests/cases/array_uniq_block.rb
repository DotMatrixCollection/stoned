p ["foo", "bar", "FOO", "baz", "BAR"].uniq { |s| s.downcase }
p [1, 2, -1, 3, -2, -3].uniq { |n| n.abs }
p [{a: 1}, {a: 2}, {b: 1}].uniq { |h| h.keys.first }
p [1, 1, 2, 2, 3].uniq

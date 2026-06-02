# Enumerable#filter_map combines select and map in one pass
p [1, 2, 3, 4, 5, 6].filter_map { |n| n * 2 if n.odd? }
p ["a", "", "b", nil, "c", false].filter_map { |s| s&.upcase unless s.to_s.empty? }
p (1..10).filter_map { |n| n ** 2 if n % 3 == 0 }

# filter_map with string processing
words = ["hello", "WORLD", "foo", "BAR", "baz"]
p words.filter_map { |w| w.downcase if w == w.upcase }

# filter_map returns [] when nothing matches
p [1, 2, 3].filter_map { |n| nil }

# Comparing with select+map
data = [1, -2, 3, -4, 5]
via_filter_map = data.filter_map { |n| n * 10 if n > 0 }
via_select_map = data.select { |n| n > 0 }.map { |n| n * 10 }
p via_filter_map
p via_filter_map == via_select_map

# filter_map without block returns enumerator
p [1, 2, 3].filter_map.class

# Works on Hash entries
h = {a: 1, b: 2, c: 3, d: 4}
p h.filter_map { |k, v| [k, v * 2] if v.odd? }

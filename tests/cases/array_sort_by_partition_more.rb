items = [
  ["pear", 4],
  ["fig", 3],
  ["apple", 5],
  ["kiwi", 4],
]

p items.sort_by { |name, size| [size, name] }
p items.partition { |name, size| size == 4 }
p items.map.with_index { |pair, i| [i, pair[0]] }

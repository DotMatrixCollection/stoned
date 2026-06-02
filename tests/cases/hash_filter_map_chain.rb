# Hash filter_map, count, sum, and chained operations
inventory = {apple: 5, banana: 0, cherry: 12, date: 3, elderberry: 0}

# filter_map on hash entries
in_stock = inventory.filter_map { |k, v| k if v > 0 }
p in_stock.sort

# count with block
p inventory.count { |_, v| v > 0 }
p inventory.count { |_, v| v == 0 }

# sum values
p inventory.sum { |_, v| v }

# min_by / max_by on hash
p inventory.min_by { |_, v| v }.first
p inventory.max_by { |_, v| v }.first

# any? and all? with block
p inventory.any? { |_, v| v > 10 }
p inventory.all? { |_, v| v >= 0 }
p inventory.none? { |_, v| v < 0 }

# flat_map on hash
pairs = {x: [1, 2], y: [3, 4], z: [5]}
p pairs.flat_map { |k, vs| vs.map { |v| [k, v] } }

# each_with_object building filtered hash
result = inventory.each_with_object({}) do |(k, v), acc|
  acc[k] = v if v > 0
end
p result

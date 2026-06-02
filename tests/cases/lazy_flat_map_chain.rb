# flat_map on lazy enumerator
result = (1..5).lazy.flat_map { |n| [n, n * 2] }.first(6)
p result

# flat_map on infinite lazy range
result2 = (1..Float::INFINITY).lazy.flat_map { |n| [n, -n] }.first(6)
p result2

# chain flat_map with select
result3 = (1..10).lazy.flat_map { |n| [n, n] }.select { |n| n.odd? }.first(4)
p result3

# flat_map collapsing nested arrays from map
result4 = (1..3).lazy.map { |n| (1..n).to_a }.flat_map { |a| a }.to_a
p result4

# collect_concat is alias for flat_map
result5 = (1..4).lazy.collect_concat { |n| [n * 10] }.to_a
p result5

enum_names = [
  [1, 2, 3].min_by,
  [1, 2, 3].min_by(2),
  [1, 2, 3].max_by,
  [1, 2, 3].max_by(2),
  [1, 2, 3].sort_by,
  [1, 2, 3].minmax_by,
  [1, 2, 3].filter_map,
  [1, 2, 3].take_while,
  [1, 2, 3].drop_while
].map { |e| e.class }

p enum_names
p [1, 2, 3].min_by.to_a
p [1, 2, 3].sort_by.to_a
p [1, 2, 3].filter_map.to_a

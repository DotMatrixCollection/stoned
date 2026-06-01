h = {a: 1, b: 2}

entry_methods = [
  :find,
  :detect,
  :map,
  :collect,
  :select,
  :find_all,
  :filter,
  :reject,
  :select!,
  :filter!,
  :keep_if,
  :reject!,
  :delete_if,
  :group_by,
  :flat_map,
  :collect_concat,
  :filter_map
]

p entry_methods.map { |m| h.dup.send(m).class }
p h.map.to_a
p h.reject.to_a
p h.group_by.to_a
p h.flat_map.to_a

p h.transform_values.class
p h.transform_values.to_a
p h.transform_keys.class
p h.transform_keys.to_a

frozen = h.dup.freeze
p frozen.select!.class
p frozen.reject!.class
p frozen.transform_values!.to_a
p frozen.transform_keys!.to_a

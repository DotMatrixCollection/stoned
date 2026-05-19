arr_enum = [10, 20].each_with_index
puts arr_enum.class
puts arr_enum.to_a.inspect

hash_key_enum = {a: 1, b: 2}.each_key
puts hash_key_enum.class
puts hash_key_enum.to_a.inspect

hash_idx_enum = {a: 1, b: 2}.each_with_index
puts hash_idx_enum.class
puts hash_idx_enum.to_a.inspect

range_enum = (3..5).each_with_index
puts range_enum.class
puts range_enum.to_a.inspect

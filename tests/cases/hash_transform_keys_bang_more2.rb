h = {a: 1, b: 2}
p h.transform_keys! { |k| k.to_s }
p h

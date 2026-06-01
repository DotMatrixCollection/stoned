h = {a: 1, b: 2}
keys = []
values = []
h.each_key { |k| keys << k }
h.each_value { |v| values << v }
p keys
p values

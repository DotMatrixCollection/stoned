a = [1, nil, [2, nil, [3]]]
p a.flatten.compact
p [[nil], [1, [2]], []].flatten(2).compact

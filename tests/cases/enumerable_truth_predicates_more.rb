p [1, 2, 3].any?
p [nil, false].any?
p [1, 2, 3].any? { |n| n > 2 }
p [1, 2, 3].all? { |n| n > 0 }
p [1, nil, 3].all?
p [].none?
p [nil, false].none?

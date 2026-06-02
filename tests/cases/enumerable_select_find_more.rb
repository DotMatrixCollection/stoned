p [1, 2, 3, 4].select { |n| n.even? }
p [1, 2, 3, 4].reject { |n| n.even? }
p [1, 2, nil, 3].compact
p [1, 2, 3].find { |n| n > 1 }
p [1, 2, 3].detect { |n| n > 4 }

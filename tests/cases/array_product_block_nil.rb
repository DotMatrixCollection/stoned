r = [1, 2].product([3, 4]) { |pair| pair }
p r
collected = []
[1, 2].product([3, 4]) { |a, b| collected << a * b }
p collected

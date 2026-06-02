groups = Hash.new { |hash, key| hash[key] = [] }

groups[:a] << 1
groups[:b] << 2
groups[:a] << 3

p groups
p groups[:missing]
p groups.key?(:missing)
p groups.default_proc.call(groups, :manual)
p groups

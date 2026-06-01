out = []
[:a, :b].each_with_index { |v, i| out << [v, i] }
p out
p [:x].each_with_index.class

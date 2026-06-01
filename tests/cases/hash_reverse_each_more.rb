out = []
{a: 1, b: 2}.reverse_each { |k, v| out << [k, v] }
p out

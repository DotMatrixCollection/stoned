out = []
ret = [:a, :b, :c].combination(2) { |c| out << c }
p ret
p out

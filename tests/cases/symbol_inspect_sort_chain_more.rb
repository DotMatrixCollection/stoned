symbols = [:delta, :alpha, :beta]
p symbols.sort.map(&:inspect)
p symbols.map { |sym| sym.inspect.upcase }

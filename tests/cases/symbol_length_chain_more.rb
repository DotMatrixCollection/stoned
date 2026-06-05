symbols = [:a, :alpha, :rb]
p symbols.map { |sym| sym.length }
p symbols.select { |sym| sym.length > 1 }

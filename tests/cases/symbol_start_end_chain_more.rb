symbols = [:alpha, :beta, :alphabet]
p symbols.select { |sym| sym.to_s.start_with?("alph") }
p symbols.map { |sym| sym.to_s.end_with?("a") }

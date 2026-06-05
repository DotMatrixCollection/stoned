items = [:a, :bb, :ccc, :dd]
p items.find_index(:ccc)
p items.find_index { |sym| sym.to_s.length == 2 }
p items.index(:z)

p "alpha,beta,,gamma".split(",").each_with_index.map { |s, i|
  [i, s.empty? ? "blank" : s.upcase]
}

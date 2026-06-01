p "abc".sub(/b/, "X")
p "abc".gsub(/[ac]/, "z")
p "abc123".gsub(/(\d+)/, "[\\1]")

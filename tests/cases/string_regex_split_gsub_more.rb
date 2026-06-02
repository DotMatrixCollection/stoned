text = "alpha-12 beta-34"

p text.scan(/[a-z]+|\d+/)
p text.split(/(-|\s+)/)
p text.gsub(/([a-z]+)-(\d+)/) { "#{$2}:#{$1.upcase}" }
p "abba".sub(/b+/, "XX")

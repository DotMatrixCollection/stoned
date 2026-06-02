# String#gsub with block transforms each match
p "hello world".gsub(/\w+/) { |m| m.capitalize }
p "abc123def456".gsub(/\d+/) { |m| "(#{m})" }
p "aabbcc".gsub(/./) { |c| c.upcase + "-" }

# gsub with captures in block
p "John Smith, Jane Doe".gsub(/(\w+) (\w+)/) { "#{$2}, #{$1}" }

# sub with block replaces only first match
p "hello hello".sub(/hello/) { |m| m.upcase }
p "one two three".sub(/\w+/) { |m| m.length.to_s }

# gsub block receives match string
results = []
"cat bat hat".gsub(/[cbh]at/) { |m| results << m; m.upcase }
p results

# gsub with hash replacement
p "aeiou".gsub(/[aeiou]/, "a" => "1", "e" => "2", "i" => "3", "o" => "4", "u" => "5")
p "hello".gsub(/[aeiou]/, "a" => "@", "e" => "3", "i" => "!", "o" => "0", "u" => "v")

re = Regexp.union("=", "+=", "-=")
puts re.inspect
puts "=~" =~ re

mix = Regexp.union(["a.b", /c+d/])
puts mix.inspect
puts "a.b" =~ mix
puts "cddd" =~ mix

p "hello world".gsub(/[aeiou]/, "*")
p "foo bar".gsub(/\w+/) { |m| m.capitalize }
p "aabbcc".gsub(/(.)\1/, '\1')
p "one two three".sub(/\w+/, "zero")

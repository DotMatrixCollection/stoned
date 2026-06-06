s = "hello world foo bar"
p s.gsub("o", "0")
p s.gsub(/[aeiou]/, "*")
p s.count("aeiou")
p s.gsub(/\w+/) { |w| w.capitalize }

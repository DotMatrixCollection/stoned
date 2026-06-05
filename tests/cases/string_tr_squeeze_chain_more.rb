s = "bookkeeper"
p s.tr("aeiou", "*")
p s.squeeze
p "aabbcc".squeeze("ab")

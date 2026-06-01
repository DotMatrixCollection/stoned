re = Regexp.union("a+b", /c+/)
p re.match?("a+b")
p re.match?("ccc")
p re.match?("ab")

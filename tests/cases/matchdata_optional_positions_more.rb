match = /(\w+)(?:-(\d+))?/.match("item-42")
p match[0]
p match[1]
p match[2]
p match.begin(2)
p match.end(2)

missing = /(\w+)(?:-(\d+))?/.match("item")
p missing[2]
p missing.begin(2)
p missing.end(2)

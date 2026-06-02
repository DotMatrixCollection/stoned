# String#tr translates characters
p "hello".tr("aeiou", "*")
p "hello".tr("el", "ip")
p "hello world".tr("a-m", "A-M")

# tr with ^ negation
p "hello".tr("^aeiou", "*")
p "hello world".tr("^a-z", "")

# tr_s squeezes translated runs
p "hello".tr_s("l", "r")
p "aabbcc".tr_s("a-z", "x")

# delete removes characters
p "hello world".delete("aeiou")
p "hello world".delete("a-e")
p "hello world".delete("^a-z")

# squeeze collapses runs of same character
p "aaabbbccc".squeeze
p "aaabbbccc".squeeze("a")
p "aaabbbccc".squeeze("ab")
p "hello    world".squeeze(" ")

# count counts occurrences of chars in set
p "hello world".count("aeiou")
p "hello world".count("l")
p "hello world".count("a-z")

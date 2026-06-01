result = "aeiou".gsub(/[aeiou]/, "a" => "@", "e" => "3", "i" => "1", "o" => "0", "u" => "v")
p result

p "hello".tr("aeiou", "*")
p "hello world".tr("a-m", "A-M")
p "hello".tr("el", "ip")
p "hello".tr("^aeiou", "*")

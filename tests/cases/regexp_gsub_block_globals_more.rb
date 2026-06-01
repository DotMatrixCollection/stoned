p "abc".gsub(/./) { |m| m.upcase }
p $&
p $`
p $'

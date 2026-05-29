def test(n)
  case n
  when 1 then x = "one"
  when 2 then x = "two"
  end
  puts x.inspect
end
test(1)
test(3)

# Variable set in one arm and used after
def rgb_like(s)
  case s.length
  when 6
    code = s[0, 2].to_i(16)
  when 3
    code = (s[0] * 2).to_i(16)
  end
  code.inspect
end
puts rgb_like("ff0000")
puts rgb_like("f00")

# Caseless form
val = 42
case
when val < 0 then sign = "neg"
when val > 0 then sign = "pos"
else              sign = "zero"
end
puts sign

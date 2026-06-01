def test(first, *rest, last)
  puts "#{first} #{rest.inspect} #{last}"
end
test(1, 2, 3, 4, 5)
test(1, 5)

def multi_post(a, *b, c, d)
  puts "#{a} #{b.inspect} #{c} #{d}"
end
multi_post(1, 2, 3, 4, 5)
multi_post(1, 2, 3)

def leading_splat(*rest, last)
  puts "#{rest.inspect} #{last}"
end
leading_splat(1, 2, 3)
leading_splat(42)

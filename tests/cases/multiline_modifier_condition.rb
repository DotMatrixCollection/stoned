x = "kept" if
  true
puts x

puts "unless-kept" unless
  false

def f(flag)
  return 1 unless
    flag
  2
end

puts f(false)
puts f(true)

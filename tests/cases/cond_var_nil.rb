x = 5
if x > 100
  y = 42
end
puts y.inspect  # nil, not NameError

result = "ok"
if false
  result = "never"
  extra = "extra"
end
puts result
puts extra.inspect  # nil

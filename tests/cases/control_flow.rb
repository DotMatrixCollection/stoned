i = 0
sum = 0
while i < 5
  i += 1
  if i == 2
    next
  end
  if i == 5
    break
  end
  sum += i
end

puts sum
puts((sum > 5) && true)

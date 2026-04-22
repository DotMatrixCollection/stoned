x = 2
case x
when 1
  puts "one"
when 2
  puts "two"
when 3
  puts "three"
else
  puts "other"
end

y = "hello"
case y
when "hi", "hello"
  puts "greeting"
when "bye"
  puts "farewell"
end

# case as expression
result = case x
when 1 then "one"
when 2 then "two"
else "other"
end
puts result

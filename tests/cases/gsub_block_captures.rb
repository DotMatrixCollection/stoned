result = "hello %N world %i".gsub(/%([0-9]+)?([a-zA-Z%])/) do
  $2.upcase
end
puts result

# $1 optional group
result2 = "pad %5n".gsub(/%([0-9]+)?([a-zA-Z])/) do
  $1 ? "#{$1}-#{$2}" : $2
end
puts result2

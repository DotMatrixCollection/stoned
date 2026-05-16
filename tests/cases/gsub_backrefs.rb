puts "hello world".gsub(/(\w+)/, '\1\1')
puts "foo bar".gsub(/(\w)(\w+)/) { $1.upcase + $2 }
puts "2024-05-16".gsub(/(\d{4})-(\d{2})-(\d{2})/, '\3/\2/\1')

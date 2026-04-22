begin
  missing_method()
rescue NoMethodError
  puts "rescued"
end

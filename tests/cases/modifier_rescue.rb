begin
  value = nope rescue 42
  puts value
rescue
  puts :outer
end

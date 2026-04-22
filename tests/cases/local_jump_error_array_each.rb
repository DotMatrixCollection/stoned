begin
  [1, 2].each
rescue LocalJumpError => e
  puts e.class
  puts e.message
end

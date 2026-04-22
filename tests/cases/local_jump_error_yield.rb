def f
  yield
end

begin
  f
rescue LocalJumpError => e
  puts e.class
  puts e.message
end

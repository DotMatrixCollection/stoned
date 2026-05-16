def with_block
  yield if true
  return yield if true
end

with_block do
  puts "hit"
  42
end

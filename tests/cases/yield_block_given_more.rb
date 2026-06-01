def around
  yield 1, 2
end

p around { |a, b| a + b }
p block_given?

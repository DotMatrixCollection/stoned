def outer
  x = 4
  puts x
end

outer { |n| puts n + 1 }

def with_yield
  yield(4)
end

with_yield { |n| puts n + 1 }

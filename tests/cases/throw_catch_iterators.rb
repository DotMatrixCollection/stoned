result = catch(:found) do
  [1, 2, 3, 4, 5].each do |n|
    throw :found, n if n == 3
  end
  :not_found
end
p result

result2 = catch(:stop) do
  [1, 2, 3].map do |n|
    throw :stop, "bailed at #{n}" if n == 2
    n * 10
  end
end
p result2

# throw from nested iterator
result3 = catch(:outer) do
  catch(:inner) do
    throw :outer, "x"
  end
  "inner_done"
end
p result3

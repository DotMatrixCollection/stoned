def pair
  return 1, 2, 3
end

def passthrough(flag)
  return 4, 5 if flag
  :nope
end

def explicit_array
  return [6, 7]
end

p pair
p passthrough(true)
p passthrough(false)
p explicit_array

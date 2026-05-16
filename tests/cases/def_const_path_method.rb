module IRB
end

def IRB::Inspector(value, inc = 1)
  value + inc
end

puts IRB::Inspector(4)
puts IRB::Inspector(4, 3)

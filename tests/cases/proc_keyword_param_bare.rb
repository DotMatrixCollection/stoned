p = proc do |input, complete:|
  complete ? input + 1 : input
end

puts p.call(4, complete: true)
puts p.call(4, complete: false)

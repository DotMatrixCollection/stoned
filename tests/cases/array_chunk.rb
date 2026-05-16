puts [1,1,2,2,3].chunk { |x| x }.map { |k, v| [k, v.size] }.inspect
puts [1,2,4,5,7,8,9].chunk_while { |a,b| b == a+1 }.to_a.inspect
puts [1,2,3,7,8,9].slice_when { |a,b| b > a+1 }.to_a.inspect
puts [1,2,3,7,8,9].slice_before { |x| x > 5 }.to_a.inspect

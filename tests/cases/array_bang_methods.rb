a = [3, 1, 2]; a.sort!; p a
a = [1, 2, 3]; p a.map! { |x| x * 2 }; p a
a = [1, 2, 3, 4]; p a.select! { |x| x > 2 }; p a
a = [1, 2, 3]; p a.reject! { |x| x > 1 }; p a
p [1, nil, 2, nil, 3].compact!
p [1, 2, 3].compact!
p [1, 2, 2, 3].uniq!
p [1, 2, 3].uniq!
p [1, [2, [3]]].flatten!
p [1, 2, 3].flatten!

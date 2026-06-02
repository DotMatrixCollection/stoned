p [1,[2,[3,[4]]]].flatten.sum
p (1..100).select(&:odd?).first(5)
p (1..10).each_with_object([]) { |n, a| a << n * n if n.even? }
p [*1..5].map { |n| [n, n**2] }.to_h

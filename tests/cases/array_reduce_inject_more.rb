p [1,2,3].reduce { |acc, n| acc + n }
p [1,2,3].reduce(10) { |acc, n| acc + n }
p [1,2,3].inject(:*)
p %w[a b c d].reduce { |acc, s| acc + s }
p [[1,2],[3,4],[5,6]].reduce([]) { |acc, a| acc + a }

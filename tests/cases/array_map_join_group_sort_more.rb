p [1,2,3].map { |n| n.to_s }.join(", ")
p %w[foo bar baz].map(&:length).sort
p %w[foo bar baz].sort_by { |s| s.length }.reverse
p %w[foo bar baz].group_by { |s| s[0] }

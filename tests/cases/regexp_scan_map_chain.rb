s = "cat 3, dog 7, bird 2"
p s.scan(/\d+/).map(&:to_i)
p s.scan(/\d+/).map(&:to_i).sum
p s.scan(/[a-z]+/).map(&:upcase)
p s.scan(/(\w+) (\d+)/).map { |name, n| "#{name}=#{n}" }

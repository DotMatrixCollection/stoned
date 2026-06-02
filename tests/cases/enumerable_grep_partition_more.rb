words = %w[alpha beta atom bolt]

p words.grep(/^a/)
p words.grep_v(/a$/)
p words.partition { |word| word.length == 4 }
p [1, 2, 3, 4, 5].grep(2..4)
p [:a, "a", :b, "b"].grep(Symbol)

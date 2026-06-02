p "hello world".scan(/\w+/)
p "one 1 two 2 three 3".scan(/\d+/).map(&:to_i).sum
p "aabbbcccc".scan(/(.)\1+/).map(&:first)

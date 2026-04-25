p "one two three".scan(Regexp.new('\w+'))
p "one 1, two 2, three 3".scan(Regexp.new('(\w+) (\d+)'))
"hello world".scan(Regexp.new('\w+')) { |w| puts w }
"one 1 two 2".scan(Regexp.new('(\w+) (\d+)')) { |w, n| puts "#{w}=#{n}" }
p "abc".scan(Regexp.new('\d+'))
p "aXb".scan(Regexp.new('(a)(z)?'))

# &method(:name) for top-level methods works as a block argument
def double(x); x * 2; end
def square(x); x * x; end
def positive?(x); x > 0; end

puts [1, 2, 3].map(&method(:double)).inspect    # [2, 4, 6]
puts [1, 2, 3].map(&method(:square)).inspect    # [1, 4, 9]
puts [-1, 0, 2].select(&method(:positive?)).inspect  # [2]

# method(:puts) as block
[42, "hi"].each(&method(:puts))   # 42 \n hi

# method(:+) would fail since + is not a top-level method — this tests only user defs
def add_one(x); x + 1; end
puts (1..5).map(&method(:add_one)).inspect  # [2, 3, 4, 5, 6]

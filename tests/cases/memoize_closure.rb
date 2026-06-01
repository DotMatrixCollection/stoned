class Memoizer
  def initialize(&block)
    @cache = {}
    @fn = block
  end

  def call(n)
    @cache[n] ||= @fn.call(n)
  end
end

fib = Memoizer.new do |n|
  if n <= 1
    n
  else
    fib.call(n-1) + fib.call(n-2)
  end
end

p fib.call(10)
p fib.call(20)

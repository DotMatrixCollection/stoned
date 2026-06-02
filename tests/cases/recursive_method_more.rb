def factorial(n) = n <= 1 ? 1 : n * factorial(n - 1)
def fib(n)       = n <= 1 ? n : fib(n-1) + fib(n-2)
p factorial(5)
p factorial(10)
p (0..7).map { |n| fib(n) }

def sum(*args)
  args.inject(0, &:+)
end
puts sum(1, 2, 3)

parts = [2, 3, 4]
puts sum(*parts)
puts sum(1, *parts)

def first_and_rest(a, *rest)
  puts a
  puts rest.inspect
end
first_and_rest(*[10, 20, 30])

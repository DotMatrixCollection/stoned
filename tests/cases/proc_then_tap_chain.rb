result = 5
  .then { |x| x * 3 }
  .then { |x| x + 1 }
p result

log = []
value = [1, 2, 3]
  .tap { |a| log << a.length }
  .map { |x| x * 2 }
  .tap { |a| log << a.sum }
p value
p log

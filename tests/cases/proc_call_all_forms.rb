double = proc { |x| x * 2 }
p double.call(5)
p double.(5)
p double[5]
p double.===(5)

strict = lambda { |x| x + 10 }
p strict.call(5)
p strict.(5)
p strict[5]

p double.lambda?
p strict.lambda?

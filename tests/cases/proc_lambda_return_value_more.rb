l = ->(x) { x * 3 }
p = proc { |x| x * 4 }
p l.call(5)
p p.call(5)
p [l.lambda?, p.lambda?]

result = [1,2,3].tap { |a| p a.length }.map { |x| x * 2 }
p result

val = 5.then { |x| x * x }
p val

val2 = "hello".yield_self { |s| s.upcase + "!" }
p val2

val3 = nil.then { |x| x || "default" }
p val3

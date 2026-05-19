r, w = IO.pipe
w.write("hello")
w.close
readable, writable, errors = IO.select([r])
puts readable.first.read
puts writable.inspect
puts errors.inspect

r2, w2 = IO.pipe
result = IO.select([r2], nil, nil, 0)
puts result.nil?
w2.close
r2.close

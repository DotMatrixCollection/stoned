p catch(:done) { throw :done, 42 }
p catch(:outer) { catch(:inner) { throw :outer, "x" } }

Thread.current[:x] = 10
p Thread.current[:x]
p Thread.current.equal?(Thread.main)
p Thread.current.status

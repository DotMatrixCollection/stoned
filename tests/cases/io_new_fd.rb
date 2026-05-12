io = IO.new(1, "w")
count = io.write("via-io-new\n")
puts io.fileno
puts io.closed?
puts count
puts io.close
puts io.closed?
puts "after-close"

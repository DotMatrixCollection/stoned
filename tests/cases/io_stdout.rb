$stdout.puts "hello"
$stdout.print "ab"
$stdout.print "cd"
$stdout.puts
$stdout.write("xyz\n")
$stdout << "chain"
$stdout << "\n"
puts $stdout.sync
$stdout.sync = false
puts $stdout.fileno
STDOUT.puts "constant"

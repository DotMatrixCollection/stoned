require "io/console"
require "io/console/size"

stdin_io = IO.open(STDIN.to_i, :external_encoding => "utf-8", :internal_encoding => "-")
stdout_io = IO.open(STDOUT.to_i, "w", :external_encoding => "utf-8")

puts stdin_io.external_encoding
puts stdin_io.internal_encoding

ws = stdout_io.winsize
puts ws.is_a?(Array)
puts ws.length
puts ws[0] > 0
puts ws[1] > 0

cs = IO.console_size
puts cs.is_a?(Array)
puts cs.length
puts cs[0] > 0
puts cs[1] > 0

puts STDOUT.raw { 7 }
puts STDOUT.cooked { 9 }

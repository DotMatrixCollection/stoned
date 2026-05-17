result = system("true")
puts result

result = system("false")
puts result

output = `echo hello`
puts output.chomp

output = `echo -n world`
puts output

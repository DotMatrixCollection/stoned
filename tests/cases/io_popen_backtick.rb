# Backtick and %x{} capture command output
out = `echo hello`
p out.chomp

out2 = %x{echo world}
p out2.chomp

# $? holds exit status after backtick
`true`
p $?.success?
p $?.exitstatus

`false`
p $?.success?
p $?.exitstatus

# Multi-word command
result = `echo one two three`
p result.chomp.split

# system() runs command, returns true/false
p system("true")
p system("false")

# Process::Status
`exit 0`
status = $?
p status.class
p status.exited?
p status.exitstatus

# IO.popen for reading subprocess output
IO.popen("echo piped") do |io|
  p io.read.chomp
end

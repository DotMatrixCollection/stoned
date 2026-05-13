base = "/tmp/stoned_dir_runtime_stage5"
sub = File.join(base, "sub")
made = File.join(base, "made")

begin
  Dir.mkdir(base)
rescue Errno::EEXIST
end

begin
  Dir.mkdir(sub)
rescue Errno::EEXIST
end

begin
  Dir.mkdir(made)
rescue Errno::EEXIST
end

orig = Dir.pwd

puts Dir.pwd == File.realpath(".")
puts Dir.chdir(base) { Dir.pwd == File.realpath(base) }
puts Dir.chdir(base) { break :broke }
puts Dir.pwd == orig

Dir.chdir(sub)
puts Dir.pwd == File.realpath(sub)
Dir.chdir(orig)

puts File.realpath(".", base) == base
puts File.realpath("sub", base) == sub
puts File.realpath(made) == made
puts File.realpath(__dir__) == File.realpath("tests/cases")

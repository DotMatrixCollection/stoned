p :hello.to_s
p :upcase.to_proc.call("hello")
p :length.to_proc.call("hello world")
p [:hello, :world].map(&:to_s)
p [:hello, :world, :foo].select { |s| s.length > 4 }
p :hello.inspect
p :hello == :hello
p :hello == :world
p [:b, :a, :c].sort

class Writer
  def initialize
    @buf = ""
  end

  def write(s)
    @buf = @buf + s
  end

  def buf
    @buf
  end
end

w = Writer.new
$stdout = w
puts "hello"
print "x"
p 1
STDOUT.puts w.buf

class Writer
  def write(s)
    STDOUT.write(s)
  end
end

$stderr = Writer.new
nil.foo

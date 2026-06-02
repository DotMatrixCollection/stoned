class Stack
  def initialize = (@data = [])
  def push(v)    = @data.push(v)
  def pop        = @data.pop
  def peek       = @data.last
  def empty?     = @data.empty?
  def size       = @data.size
end
s = Stack.new
s.push(1); s.push(2); s.push(3)
p s.peek
p s.pop
p s.size
p s.empty?

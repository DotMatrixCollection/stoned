class X
  attr_accessor :prompt
  public :prompt
end

x = X.new
x.prompt = 9
puts x.prompt

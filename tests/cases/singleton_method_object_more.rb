obj = Object.new
def obj.answer; 42; end
p obj.singleton_methods.include?(:answer)
p obj.method(:answer).call

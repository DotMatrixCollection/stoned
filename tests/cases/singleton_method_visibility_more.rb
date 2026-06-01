obj = Object.new
def obj.visible; :visible; end
obj.define_singleton_method(:other) { :other }

p obj.singleton_methods.include?(:visible)
p obj.singleton_methods.include?(:other)
p obj.other

module ConstRoot
  VALUE = 1
  module Child
    VALUE = 2
  end
end

p ConstRoot.const_get(:VALUE)
p ConstRoot::Child.const_get(:VALUE)
p ConstRoot.constants.include?(:Child)

module ConstGetOuter
  VALUE = "outer"

  module Inner
    VALUE = "inner"
  end
end

p ConstGetOuter.const_get(:VALUE)
p ConstGetOuter.const_get(:Inner).const_get(:VALUE)
p ConstGetOuter.const_defined?(:Inner)
p ConstGetOuter::Inner.const_defined?(:Missing)

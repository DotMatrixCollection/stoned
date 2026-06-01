m = Module.new
m.const_set(:VALUE, 123)
p m.const_get(:VALUE)
p m.const_defined?(:VALUE)
p m.constants.include?(:VALUE)
